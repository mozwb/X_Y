#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  XMemStats.h — 内存统计（声明）
//
//  职责边界（重要）：
//    本文件【只记账】，不认识后端、不认识分配、不认识对象。
//    它只暴露几个动作：
//      onAllocate(size, backend)   —— 有人分配了，记一笔
//      onDeallocate(size, backend) —— 有人释放了，销一笔
//    内存从哪来、怎么还，全是 XMemBackend.h 的事。
//    实现见 XMemStats.cpp。
//
//  三个必须分清的数字（别混成一个，这是验收泄漏的关键）：
//    UsedBytes  用户用量：真正 allocate 出去的字节。
//               ★ 验收"有没有泄漏"看它回到 0，不看 capacity。
//    Capacity   后端向 OS 要来的总字节。slab 会缓存 chunk 不还给 OS，
//               所以 capacity 不归零是【正常的】，不是泄漏。
//    Overhead   元数据 + header 的开销。调优时看，不影响泄漏判断。
//
//  统计维度（四个，缺一不可）：
//    ① 按后端   —— CRT 用了多少 / Slab 用了多少 → 决定"要不要切"
//    ② 按大小档 —— 各尺寸区间的分配次数 → 日后定策略的数据依据
//    ③ 峰值+当前 —— 看波动（UI 场景会周期性涨落）
//    ④ 未释放块 —— 泄漏自检
//
//  命名区分：
//    MemoryCounter —— 活的计数器（原子内部状态）
//    MemoryStats   —— 只读快照（POD，可安全传阅）
// ═════════════════════════════════════════════════════════════════════════════

#include "XMemTypes.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace X_Y
{

    // ── 单个后端的统计量（快照用 POD）──────────────────────────────────────
    struct BackendStats
    {
        uint64_t UsedBytes = 0;     // 用户用量
        uint64_t CapacityBytes = 0; // 向 OS 要来的容量（0 = 不可知）
        uint64_t AllocCount = 0;    // 分配次数
        uint64_t FreeCount = 0;     // 释放次数
        uint64_t LiveCount = 0;     // 未释放块数（Alloc - Free），正常时 = 0

        // 字面上的"泄漏线索"
        uint64_t LeakedBlocks() const { return LiveCount; }
        uint64_t LeakedBytes() const { return UsedBytes; }
    };

    // ── 单个大小档的统计量（快照用 POD）────────────────────────────────────
    // 记录"该尺寸区间分配了多少次、峰值多少字节"。
    // 用途：定策略前先看清程序在分配什么尺寸（数据驱动，而非拍脑袋）。
    struct SizeClassStats
    {
        SizeClass Class = SizeClass::Tiny;
        uint64_t AllocCount = 0;   // 该区间累计分配次数
        uint64_t FreeCount = 0;    // 该区间累计释放次数
        uint64_t CurrentBytes = 0; // 该区间当前占用字节
        uint64_t PeakBytes = 0;    // 该区间峰值占用字节
    };

    // ── MemoryStats：只读快照（POD，与 Counter 分离）──────────────────────
    // Counter 是"活的"，Stats 是"照下来那一刻的"。拿到 Stats 后可随便
    // 传阅/打印/比较，不受后续分配影响。
    struct MemoryStats
    {
        uint64_t UsedBytes = 0;
        uint64_t PeakBytes = 0;
        uint64_t OverheadBytes = 0;
        uint64_t TotalAllocs = 0;
        uint64_t TotalFrees = 0;
        uint64_t LiveBlocks = 0;
        uint64_t OOMCount = 0;

        // ── 基线（可选）：调过 markBaseline() 后，上面各值已是"相对基线"的 ──
        bool HasBaseline = false;
        uint64_t EarlyAllocs = 0;       // 基线之前发生的分配次数（开机成本）
        uint64_t PreBaselineFrees = 0;  // 跨基线释放次数（基线前内存被回收）
        uint64_t BaselineUsedBytes = 0; // 基线当时的累计用量

        BackendStats Backends[static_cast<uint32_t>(BackendType::Count)];
        SizeClassStats SizeClasses[static_cast<uint32_t>(SizeClass::Count)];

        BackendStats Get(BackendType t) const
        {
            const uint32_t i = static_cast<uint32_t>(t);
            return i < static_cast<uint32_t>(BackendType::Count) ? Backends[i]
                                                                 : BackendStats{};
        }

        SizeClassStats Get(SizeClass c) const
        {
            const uint32_t i = static_cast<uint32_t>(c);
            return i < static_cast<uint32_t>(SizeClass::Count) ? SizeClasses[i]
                                                               : SizeClassStats{};
        }
    };

    // ── MemoryCounter：统计器 ───────────────────────────────────────────────
    // 线程安全（原子计数）。★ 自身零堆分配（固定数组 + 原子），
    // 因此可以在自举期（门面首次 allocate）安全使用，不会递归。
    //
    // 声明与实现分离：所有方法在 XMemStats.cpp。
    class MemoryCounter
    {
    public:
        MemoryCounter() = default;

        MemoryCounter(const MemoryCounter &) = delete;
        MemoryCounter &operator=(const MemoryCounter &) = delete;

        // ── 记账（门面在 allocate/deallocate 成功后调用）──
        // size 传【用户请求的字节数】，不是后端块大小。
        void onAllocate(uint64_t size, BackendType backend);
        void onDeallocate(uint64_t size, BackendType backend);

        // 后端容量上报（后端扩容时调；不可知的后端不调）
        void onCapacityChange(uint64_t capacity, BackendType backend);

        void onOverhead(uint64_t bytes); // header / 元数据
        void onOOM();

        // ── 读取（绝对累计值，不含基线偏移）──
        uint64_t UsedBytes() const;
        uint64_t PeakBytes() const;
        uint64_t OverheadBytes() const;
        uint64_t TotalAllocs() const;
        uint64_t TotalFrees() const;
        uint64_t OOMCount() const;

        BackendStats Get(BackendType t) const;
        SizeClassStats Get(SizeClass c) const;

        // 未释放块总数（绝对；>0 说明有对象没回收）
        uint64_t LiveBlocks() const;

        // ── 基线（统计零点校准 / "去皮"）──
        // 为什么需要：全局 operator new 重载后，程序启动期的分配（全局对象、
        // CRT 自身的环境/locale/stdio 缓冲）也会被统计进来，导致
        //   - main 刚进去 UsedBytes 就不是 0（懵）
        //   - 泄漏自检 liveBlocks()==0 永远不成立（判据失效）
        // 解法：在 main 开头调一次 markBaseline()，之后所有读取都减去基线。
        //
        // ⚠️ 基线【只影响读取】，不影响分配行为。
        // ⚠️ 跨基线释放（基线前分配、基线后释放）会做钳制，不让数字下溢。
        void markBaseline();
        void clearBaseline();
        bool hasBaseline() const;

        // ── 基线相对读取 ──
        uint64_t UsedBytesRelative() const;
        uint64_t PeakBytesRelative() const;
        uint64_t LiveBlocksRelative() const;

        uint64_t EarlyAllocs() const;
        uint64_t PreBaselineFrees() const;

        // ── 快照 / 重置 / 打印 ──
        MemoryStats Snapshot() const;
        void Reset();
        std::string ToString() const;

    private:
        // 槽位按枚举大小固定开。用固定数组而非 vector，
        // 保证统计器本身零堆分配 —— 它要在自举期就能用。
        static constexpr uint32_t kSlots = static_cast<uint32_t>(BackendType::Count);
        static constexpr uint32_t kSizeSlots = static_cast<uint32_t>(SizeClass::Count);

        // ⚠️ Slot 是嵌套类型，所以下面的访问器【不能】也叫 Slot()：
        //    类作用域内 Slot 会先解析成函数名 → "Slot does not name a type"。
        //    故访问器取名 slotOf / sizeSlotOf。
        struct Slot
        {
            std::atomic<uint64_t> UsedBytes{0};
            std::atomic<uint64_t> CapacityBytes{0};
            std::atomic<uint64_t> AllocCount{0};
            std::atomic<uint64_t> FreeCount{0};
        };

        struct SizeSlot
        {
            std::atomic<uint64_t> AllocCount{0};
            std::atomic<uint64_t> FreeCount{0};
            std::atomic<uint64_t> CurrentBytes{0};
            std::atomic<uint64_t> PeakBytes{0};
        };

        static uint32_t Idx(BackendType t);
        static uint32_t SizeIdx(SizeClass c);

        Slot &slotOf(BackendType t) { return m_Slots[Idx(t)]; }
        const Slot &slotOf(BackendType t) const { return m_Slots[Idx(t)]; }

        SizeSlot &sizeSlotOf(SizeClass c) { return m_SizeSlots[SizeIdx(c)]; }
        const SizeSlot &sizeSlotOf(SizeClass c) const { return m_SizeSlots[SizeIdx(c)]; }

        Slot m_Slots[kSlots];
        SizeSlot m_SizeSlots[kSizeSlots];

        std::atomic<uint64_t> m_UsedTotal{0};
        std::atomic<uint64_t> m_PeakTotal{0};
        std::atomic<uint64_t> m_Overhead{0};
        std::atomic<uint64_t> m_TotalAllocs{0};
        std::atomic<uint64_t> m_TotalFrees{0};
        std::atomic<uint64_t> m_OOMCount{0};

        // 基线
        std::atomic<bool> m_HasBaseline{false};
        uint64_t m_BaselineUsed = 0;
        uint64_t m_BaselinePeak = 0;
        uint64_t m_BaselineAllocs = 0;
        uint64_t m_BaselineFrees = 0;
        uint64_t m_EarlyAllocs = 0;
    };

} // namespace X_Y
