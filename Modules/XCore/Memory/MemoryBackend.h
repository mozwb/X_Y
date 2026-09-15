#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  MemoryBackend.h — 内存后端接口（第 2 步：只做后端）
//
//  职责边界（重要）：
//    本文件【只负责"字节从哪来、怎么还"】。
//    不认识统计、不认识对象构造、不认识 UI。
//    统计在 MemoryStats.h，门面在 Memory.h。
//
//  铁律（改本文件前必读）：
//    1. 后端内部【只能】使用 ::malloc / ::free（或 OS 原语）拿内存，
//       绝对不许调用 Memory 门面、更不许用 std::vector / std::string。
//       理由：门面首次使用时要"懒创建"后端，若后端自己又回调门面，
//             就是自举递归（bootstrap recursion）→ 死循环 / UB。
//       将来 SlabBackend 的元数据（chunk 表、free list）也必须走 malloc。
//    2. 后端不保存任何需要"运行期构造"的全局状态。
//    3. 每个后端必须能回答 own(ptr)：这个指针是不是我发的？
//       —— 因为 deallocate(ptr) 不带后端参数，只能靠它判断归属。
//
//  当前进度：
//    ✅ CrtBackend   —— 标准库堆（默认后端，标准库兜底）
//    ⏳ SlabBackend  —— 待迁移（旧 Memory/XMemory.h 里的 slab 实现搬过来）
// ═════════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstddef>
#include <cstdlib>

namespace X_Y
{

    // ── OOM 处理策略 ─────────────────────────────────────────────────────────
    // 放在这里（而不是门面）是因为"分配不到内存怎么办"属于分配层策略；
    // 门面与后端都要引用它，放上游避免循环依赖。
    // （从旧 XMemory.h 迁来，语义不变）
    enum class OOMAction
    {
        Abort,      // 开发期：超限直接 terminate，尽早暴露问题
        ReturnNull, // 发布版：超限返回 nullptr，调用方自己处理
        Expand,     // 调试期：突破预算继续分配，在统计中标记为超限
    };

    // ── 后端类型标签 ─────────────────────────────────────────────────────────
    // 用于 allocate(size, type) 选择后端，也是"哪个后端"的可读名字。
    enum class BackendType : uint8_t
    {
        Default = 0, // 走门面配置的默认后端（当前 = Crt）
        Crt = 1,     // 标准库堆（malloc/free）
        Slab = 2,    // 预留：自研 slab 分配器（未接入）
        Count
    };

    inline const char *BackendName(BackendType t)
    {
        switch (t)
        {
        case BackendType::Default:
            return "Default";
        case BackendType::Crt:
            return "Crt";
        case BackendType::Slab:
            return "Slab";
        default:
            return "Unknown";
        }
    }

    // ── 后端接口 ─────────────────────────────────────────────────────────────
    // 纯抽象：门面持有 IMemoryBackend*，外界永远看不到具体后端。
    //
    // ⚠️ allocate 与 deallocate 的分工：
    //    allocate  → 门面把 BackendType 解析成具体后端，再调到这里。
    //    deallocate→ 门面【无法】知道该找哪个后端（delete 不传回构造参数），
    //                所以门面要么靠 header、要么挨个问 own()。
    //                本接口保留 size 参数是为了让后端能自己记账/还给 OS。
    class IMemoryBackend
    {
    public:
        virtual ~IMemoryBackend() = default;

        // 分配 size 字节。返回的指针必须满足 alignof(std::max_align_t)。
        // 失败返回 nullptr（由门面决定抛 bad_alloc 还是返回空）。
        virtual void *allocate(uint64_t size) = 0;

        // 归还。size 是当初 allocate 的字节数（可能为 0 = 未知）。
        // 实现必须容忍 size == 0（门面在无法得知大小时会传 0）。
        virtual void deallocate(void *ptr, uint64_t size) = 0;

        // 这个指针是不是本后端发出的？
        // ★ 这是 deallocate(ptr) 能否只靠地址判断归属的关键接口。
        //   CrtBackend 无法精确回答（见其实现注释），故门面改用 header 方案；
        //   保留此接口供 SlabBackend 等"可精确自报"的后端使用。
        virtual bool own(void *ptr) const = 0;

        // 后端名（调试/统计用）
        virtual const char *name() const = 0;

        // 后端向 OS 拿到的总容量（字节）。统计"我占了多少内存"用。
        virtual uint64_t capacity() const = 0;
    };

    // ── CrtBackend：标准库堆 ─────────────────────────────────────────────────
    // 默认后端。目的就是"标准库兜底"：
    //   - 性能与别人直接用 new/malloc 完全一致（就是同一个堆）；
    //   - 行为可预期，出问题不是我们的分配器问题；
    //   - 自研 slab 只在需要优化的热点上按需启用，效果不好随时切回来。
    //
    // ⚠️ 本后端只用 ::malloc / ::free，且【不保存任何运行期状态】，
    //    因此它是平凡类：可以在静态初始化期安全使用，无自举问题。
    class CrtBackend final : public IMemoryBackend
    {
    public:
        void *allocate(uint64_t size) override
        {
            if (size == 0)
                return nullptr;
            return std::malloc(static_cast<std::size_t>(size));
        }

        void deallocate(void *ptr, uint64_t /*size*/) override
        {
            // 标准库 free 自己知道块大小，不需要 size。
            std::free(ptr);
        }

        bool own(void * /*ptr*/) const override
        {
            // ❌ 标准库堆没有"这个指针是不是我的"的 API。
            //    Windows 的 HeapValidate/HeapWalk 极慢，不能每次释放都调用。
            //    所以 CrtBackend 一律返回 false —— 门面据此事先知道：
            //    "归属判断不能依赖后端自报，必须用 header 记录"。
            return false;
        }

        const char *name() const override { return "CrtBackend"; }

        uint64_t capacity() const override
        {
            // 标准库堆不向外暴露容量。返回 0 表示"不可知"，
            // 门面统计里这一项显示 0，不代表没占用。
            return 0;
        }
    };

} // namespace X_Y
