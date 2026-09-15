#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  Memory.h — 内存门面（Facade，第 2 步）
//
//  这是外界【唯一】要认识的文件。分工：
//      Memory.h          ← 门面：外界入口，选后端、记账、归属判断
//      MemoryBackend.h   ← 后端：字节从哪来、怎么还
//      MemoryStats.h     ← 统计：只记账
//
//  ── 设计要点（改之前必读）──
//
//  1. 为什么必须有 header（分配头）：
//     `delete p` 时 C++ 【不会】把构造时的额外参数传回来，所以门面
//     无法知道 p 当初走的是哪个后端。因此在分配时把"我是谁"记在
//     返回指针前面，deallocate 时往回读一眼就知道该找谁。
//     这解决了"混用"：分配时你选后端，释放时你不用记。
//
//  2. 为什么门面必须是平凡（POD）的：
//     门面可能在静态初始化期（甚至更早）被触达。若门面自己持有
//     std::vector / std::string 之类需要分配的成员，构造时就要分配，
//     而分配又要进门面 → 递归自举 → UB。
//     所以：门面只持有指针 + POD，后端与计数器都在指针后面懒创建。
//
//  3. 自举铁律：
//     后端、统计器、header 池内部【只用 ::malloc / ::free】，
//     绝不回调门面。否则第一次 allocate 就会死循环。
//
//  ── 用法 ──
//      auto& m = Memory::Instance();
//
//      void* p  = m.allocate(1024);                      // 默认后端
//      void* q  = m.allocate(1024, BackendType::Slab);   // 指定后端
//      m.deallocate(p);                                  // 不用记后端！
//
//      auto* obj = m.Allocate<LogViewer>(BackendType::Crt, args...);
//      m.Deallocate(obj);
//
//      // 旧名兼容（现有代码如 Buffer 零改动）
//      void* r = m.Alloc(1024);
//      m.Free(r);
// ═════════════════════════════════════════════════════════════════════════════

#include "MemoryBackend.h"
#include "MemoryStats.h"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <new>
#include <utility>

namespace X_Y
{

    // ── 分配头 ───────────────────────────────────────────────────────────────
    // 布局：  [ AllocHeader ][ 用户数据 ... ]
    //                        ↑ 返回给用户的地址
    //
    // 每个分配前面都藏一份，用来回答三件事：
    //   magic    —— 这个指针是不是我们发的？（快速合法性检查）
    //   backend  —— 该还给哪个后端
    //   size     —— 原始请求大小（还给后端，并用于统计销账）
    //
    // ⚠️ 对齐：header 必须占整数倍对齐字节，保证后面用户数据仍满足
    //    alignof(max_align_t)。故 kHeaderSize 向上取整到 16。
    struct AllocHeader
    {
        uint32_t magic;    // kMagic 标记
        uint8_t backend;   // BackendType
        uint8_t pad[3];
        uint64_t size;     // 用户请求的字节数
    };

    inline constexpr uint32_t kAllocMagic = 0x58595F4D; // 'X','Y','_','M'
    inline constexpr uint64_t kHeaderRaw = sizeof(AllocHeader); // 16
    inline constexpr uint64_t kHeaderSize =
        (kHeaderRaw + 15) & ~static_cast<uint64_t>(15);

    // ── Memory：门面 ─────────────────────────────────────────────────────────
    class Memory
    {
    public:
        // ── 单例（平凡：不构造任何需要分配的东西）──
        static Memory &Instance()
        {
            // s_Inst 是平凡对象：无虚函数、无需要堆分配的成员
            // （m_Counter 是固定数组 + 原子，m_Backends 是裸指针数组）。
            // 因此它的初始化不依赖任何运行期分配，不存在"构造顺序"问题。
            // 编译期常量初始化 → 在任何 allocate 之前就已就绪。
            static Memory s_Inst;
            return s_Inst;
        }

        // ── 配置 ──
        void setDefaultBackend(BackendType t) { m_DefaultBackend = t; }
        BackendType defaultBackend() const { return m_DefaultBackend; }

        void setMaxBytes(uint64_t bytes) { s_MaxBytes = bytes; }
        uint64_t maxBytes() const { return s_MaxBytes; }

        void setOOMAction(OOMAction a) { s_OOMAction = a; }
        OOMAction oomAction() const { return s_OOMAction; }

        // ═════════════════════════════════════════════════════════════════════
        //  核心分配 / 释放（STL 风格命名）
        // ═════════════════════════════════════════════════════════════════════

        // 分配 size 字节。type = Default 时走默认后端。
        // 失败行为由 OOMAction 决定；Abort 之外统一返回 nullptr。
        void *allocate(uint64_t size, BackendType type = BackendType::Default)
        {
            if (size == 0)
                return nullptr;

            const BackendType resolved = Resolve(type);
            IMemoryBackend *backend = backendFor(resolved);
            if (!backend)
                return nullptr;

            // 预算检查
            if (s_MaxBytes != 0)
            {
                const uint64_t used = m_Counter.UsedBytes();
                const uint64_t need = size + kHeaderSize;
                if (used + need > s_MaxBytes)
                {
                    m_Counter.onOOM();
                    if (s_OOMAction == OOMAction::Abort)
                    {
                        std::fprintf(stderr,
                                     "[Memory] OOM: budget %llu exceeded "
                                     "(used=%llu, need=%llu)\n",
                                     (unsigned long long)s_MaxBytes,
                                     (unsigned long long)used,
                                     (unsigned long long)need);
                        std::terminate();
                    }
                    if (s_OOMAction == OOMAction::ReturnNull)
                        return nullptr;
                    // Expand：继续分配（已记 OOM）
                }
            }

            // ① 多要一块 header 的空间
            void *raw = backend->allocate(size + kHeaderSize);
            if (!raw)
            {
                m_Counter.onOOM();
                if (s_OOMAction == OOMAction::Abort)
                    std::terminate();
                return nullptr;
            }

            // ② 在头部写清"我是谁"
            auto *hdr = static_cast<AllocHeader *>(raw);
            hdr->magic = kAllocMagic;
            hdr->backend = static_cast<uint8_t>(resolved);
            hdr->size = size;

            // ③ 返回 header 之后的用户区
            void *user = static_cast<uint8_t *>(raw) + kHeaderSize;

            m_Counter.onAllocate(size, resolved);
            m_Counter.onOverhead(kHeaderSize);
            return user;
        }

        // 归还。★ 不需要传后端、不需要传大小 —— 全靠 header。
        void deallocate(void *ptr, uint64_t size = 0)
        {
            if (!ptr)
                return;

            // 兼容：调用方若传了 size，仅用于校验（不参与归还逻辑）
            void *raw = static_cast<uint8_t *>(ptr) - kHeaderSize;
            auto *hdr = static_cast<AllocHeader *>(raw);

            if (hdr->magic != kAllocMagic)
            {
                // 不是我们发的指针：可能是外部 CRT 指针误入。
                // 不做处理并报告 —— 静默 free 会破坏别的堆。
                std::fprintf(stderr,
                             "[Memory] deallocate: bad magic %p "
                             "(not allocated by Memory facade)\n",
                             ptr);
                return;
            }

            const uint64_t realSize = hdr->size;
            if (size != 0 && size != realSize)
            {
                std::fprintf(stderr,
                             "[Memory] deallocate: size mismatch %p "
                             "(given=%llu, actual=%llu)\n",
                             ptr, (unsigned long long)size,
                             (unsigned long long)realSize);
            }

            const BackendType bt = static_cast<BackendType>(hdr->backend);
            IMemoryBackend *backend = backendFor(bt);

            hdr->magic = 0; // 防重复释放时误判（双 free 会在这里被挡下）

            m_Counter.onDeallocate(realSize, bt);
            if (backend)
                backend->deallocate(raw, realSize + kHeaderSize);
        }

        // ═════════════════════════════════════════════════════════════════════
        //  对象级便捷接口（分配 + 构造 / 析构 + 归还）
        // ═════════════════════════════════════════════════════════════════════

        template <typename T, typename... Args>
        T *Allocate(BackendType type, Args &&...args)
        {
            void *mem = allocate(sizeof(T), type);
            if (!mem)
                return nullptr;
            // 构造抛异常时要把内存还回去，否则泄漏
            try
            {
                return new (mem) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                deallocate(mem, sizeof(T));
                throw;
            }
        }

        // 默认后端版（参数表里没有 BackendType 时用这个）
        template <typename T, typename... Args>
        T *Allocate(Args &&...args)
        {
            return Allocate<T>(BackendType::Default, std::forward<Args>(args)...);
        }

        template <typename T>
        void Deallocate(T *ptr)
        {
            if (!ptr)
                return;
            ptr->~T();
            deallocate(ptr, sizeof(T));
        }

        // ═════════════════════════════════════════════════════════════════════
        //  旧名兼容层（现有调用点零改动：Buffer 等）
        // ═════════════════════════════════════════════════════════════════════
        void *Alloc(uint64_t size) { return allocate(size); }
        void Free(void *ptr) { deallocate(ptr, 0); }

        template <typename T, typename... Args>
        T *AllocT(Args &&...args)
        {
            return Allocate<T>(BackendType::Default, std::forward<Args>(args)...);
        }

        template <typename T>
        void FreeT(T *ptr) { Deallocate(ptr); }

        // ═════════════════════════════════════════════════════════════════════
        //  统计
        // ═════════════════════════════════════════════════════════════════════
        MemoryStats stats() const { return m_Counter.Snapshot(); }
        MemoryStats GetStats() const { return m_Counter.Snapshot(); } // 旧名兼容

        uint64_t UsedBytes() const { return m_Counter.UsedBytes(); }
        uint64_t UsedCapacity() const { return m_Counter.PeakBytes(); }
        uint64_t TotalAllocs() const { return m_Counter.TotalAllocs(); }
        uint64_t TotalFrees() const { return m_Counter.TotalFrees(); }
        uint64_t OOMCount() const { return m_Counter.OOMCount(); }

        // ★ 泄漏自检：还活着的块数。正常程序收尾时必须是 0。
        uint64_t liveBlocks() const { return m_Counter.LiveBlocks(); }
        bool hasLeaks() const { return liveBlocks() != 0; }

        bool UnderBudget() const
        {
            return s_MaxBytes == 0 || m_Counter.UsedBytes() < s_MaxBytes;
        }

        std::string ToString() const { return m_Counter.ToString(); }

        // ═════════════════════════════════════════════════════════════════════
        //  后端管理
        // ═════════════════════════════════════════════════════════════════════
        // 注册后端（拥有权：不接管，调用方自己保证生命周期长于使用期）。
        // 预设：Crt 后端在首次使用时自动就位。
        void registerBackend(BackendType t, IMemoryBackend *backend)
        {
            const uint32_t i = static_cast<uint32_t>(t);
            if (i < kBackendSlots)
                m_Backends[i].store(backend, std::memory_order_release);
        }

        bool hasBackend(BackendType t) const
        {
            const uint32_t i = static_cast<uint32_t>(t);
            if (i >= kBackendSlots)
                return false;
            if (t == BackendType::Crt)
                return true; // Crt 永远可用（静态兜底）
            return m_Backends[i].load(std::memory_order_acquire) != nullptr;
        }

        IMemoryBackend *backendFor(BackendType t)
        {
            const uint32_t i = static_cast<uint32_t>(t);
            if (i >= kBackendSlots)
                return nullptr;

            // Crt 后端是平凡类（只有 ::malloc/::free，无状态无构造），
            // 放在函数内 static：常量初始化，首次取用时已就绪，多线程安全
            // （C++11 起函数内 static 的初始化是线程安全的，且这里其实是
            //   静态初始化，连竞争都没有）。
            static CrtBackend s_Crt;

            if (t == BackendType::Crt && m_Backends[i] == nullptr)
            {
                // 原子发布：多线程同时首次 allocate 时，
                // 两个线程都写同一个值（都指向 &s_Crt），无害。
                m_Backends[i].store(&s_Crt, std::memory_order_release);
            }
            return m_Backends[i].load(std::memory_order_acquire);
        }

        // 关闭（程序收尾调用；释放后端缓存并报告未回收块）
        void Shutdown()
        {
            if (m_Counter.LiveBlocks() != 0)
            {
                std::fprintf(stderr,
                             "[Memory] Shutdown: %llu blocks still alive "
                             "(leak suspected)\n",
                             (unsigned long long)m_Counter.LiveBlocks());
            }
            m_Shutdown = true;
        }

    private:
        Memory() = default;
        ~Memory() = default;
        Memory(const Memory &) = delete;
        Memory &operator=(const Memory &) = delete;

        BackendType Resolve(BackendType t) const
        {
            if (t == BackendType::Default || t == BackendType::Count)
                return m_DefaultBackend;
            return t;
        }

        static constexpr uint32_t kBackendSlots =
            static_cast<uint32_t>(BackendType::Count);

        // ⚠️ 只放平凡成员：门面必须能常量初始化
        BackendType m_DefaultBackend = BackendType::Crt;
        bool m_Shutdown = false;
        std::atomic<IMemoryBackend *> m_Backends[kBackendSlots] = {};
        MemoryCounter m_Counter; // 自身零堆分配（固定数组 + 原子），安全

        // 全局配置（inline static，零初始化）
        inline static uint64_t s_MaxBytes = 0; // 0 = 不限预算
        inline static OOMAction s_OOMAction = OOMAction::Abort;
    };

} // namespace X_Y
