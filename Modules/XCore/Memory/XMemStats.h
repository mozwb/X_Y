#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  XMemStats.h — 内存统计
//
//  职责边界（重要）：
//    本文件【只记账】，不认识后端、不认识分配、不认识对象。
//    它只暴露几个动作：
//      onAllocate(size, backend)   —— 有人分配了，记一笔
//      onDeallocate(size, backend) —— 有人释放了，销一笔
//    内存从哪来、怎么还，全是 XMemBackend.h 的事。
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
#include <sstream>

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
        uint64_t AllocCount = 0;    // 该区间累计分配次数
        uint64_t FreeCount = 0;     // 该区间累计释放次数
        uint64_t CurrentBytes = 0;  // 该区间当前占用字节
        uint64_t PeakBytes = 0;     // 该区间峰值占用字节
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
        uint64_t EarlyAllocs = 0;         // 基线之前发生的分配次数（开机成本）
        uint64_t PreBaselineFrees = 0;    // 跨基线释放次数（会导致基线前内存被回收）
        uint64_t BaselineUsedBytes = 0;   // 基线当时的累计用量

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
    class MemoryCounter
    {
    public:
        // ── 记账（门面在 allocate/deallocate 成功后调用）──
        // size 传【用户请求的字节数】，不是后端块大小。
        void onAllocate(uint64_t size, BackendType backend)
        {
            slotOf(backend).UsedBytes.fetch_add(size, std::memory_order_relaxed);
            slotOf(backend).AllocCount.fetch_add(1, std::memory_order_relaxed);
            m_TotalAllocs.fetch_add(1, std::memory_order_relaxed);
            m_UsedTotal.fetch_add(size, std::memory_order_relaxed);

            // 大小档维度（定策略的数据依据）
            SizeSlot &sc = sizeSlotOf(ClassifySize(size));
            sc.AllocCount.fetch_add(1, std::memory_order_relaxed);
            const uint64_t curBytes =
                sc.CurrentBytes.fetch_add(size, std::memory_order_relaxed) + size;
            uint64_t scPeak = sc.PeakBytes.load(std::memory_order_relaxed);
            while (curBytes > scPeak &&
                   !sc.PeakBytes.compare_exchange_weak(scPeak, curBytes,
                                                       std::memory_order_relaxed))
                ;

            // peak：只在总量上记（比较交换，容忍竞态下轻微低估）
            uint64_t cur = m_UsedTotal.load(std::memory_order_relaxed);
            uint64_t peak = m_PeakTotal.load(std::memory_order_relaxed);
            while (cur > peak &&
                   !m_PeakTotal.compare_exchange_weak(peak, cur,
                                                      std::memory_order_relaxed))
                ;
        }

        void onDeallocate(uint64_t size, BackendType backend)
        {
            slotOf(backend).UsedBytes.fetch_sub(size, std::memory_order_relaxed);
            slotOf(backend).FreeCount.fetch_add(1, std::memory_order_relaxed);
            m_TotalFrees.fetch_add(1, std::memory_order_relaxed);
            m_UsedTotal.fetch_sub(size, std::memory_order_relaxed);

            SizeSlot &sc = sizeSlotOf(ClassifySize(size));
            sc.FreeCount.fetch_add(1, std::memory_order_relaxed);
            // 钳制：跨基线/异常情况下不让它下溢成天文数字
            uint64_t cur = sc.CurrentBytes.load(std::memory_order_relaxed);
            sc.CurrentBytes.store(cur > size ? cur - size : 0,
                                  std::memory_order_relaxed);
        }

        // 后端容量上报（后端扩容时调；不可知的后端不调）
        void onCapacityChange(uint64_t capacity, BackendType backend)
        {
            slotOf(backend).CapacityBytes.store(capacity, std::memory_order_relaxed);
        }

        void onOverhead(uint64_t bytes) // header / 元数据
        {
            m_Overhead.fetch_add(bytes, std::memory_order_relaxed);
        }

        void onOOM() { m_OOMCount.fetch_add(1, std::memory_order_relaxed); }

        // ── 读取 ──
        uint64_t UsedBytes() const { return m_UsedTotal.load(std::memory_order_relaxed); }
        uint64_t PeakBytes() const { return m_PeakTotal.load(std::memory_order_relaxed); }
        uint64_t OverheadBytes() const { return m_Overhead.load(std::memory_order_relaxed); }
        uint64_t TotalAllocs() const { return m_TotalAllocs.load(std::memory_order_relaxed); }
        uint64_t TotalFrees() const { return m_TotalFrees.load(std::memory_order_relaxed); }
        uint64_t OOMCount() const { return m_OOMCount.load(std::memory_order_relaxed); }

        BackendStats Get(BackendType t) const
        {
            BackendStats s;
            const Slot &slot = slotOf(t);
            s.UsedBytes = slot.UsedBytes.load(std::memory_order_relaxed);
            s.CapacityBytes = slot.CapacityBytes.load(std::memory_order_relaxed);
            s.AllocCount = slot.AllocCount.load(std::memory_order_relaxed);
            s.FreeCount = slot.FreeCount.load(std::memory_order_relaxed);
            s.LiveCount = s.AllocCount - s.FreeCount;
            return s;
        }

        // 未释放块总数（>0 说明有对象没回收）
        uint64_t LiveBlocks() const
        {
            uint64_t blocks = 0;
            for (uint32_t i = 0; i < kSlots; ++i)
            {
                const Slot &slot = m_Slots[i];
                blocks += slot.AllocCount.load(std::memory_order_relaxed) -
                          slot.FreeCount.load(std::memory_order_relaxed);
            }
            return blocks;
        }

        // ═════════════════════════════════════════════════════════════════════
        //  基线（统计零点校准 / "去皮"）
        // ═════════════════════════════════════════════════════════════════════
        // 为什么需要：全局 operator new 重载后，程序启动期的分配（全局对象、
        // CRT 自身的环境/locale/stdio 缓冲）也会被统计进来，导致
        //   - main 刚进去 UsedBytes 就不是 0（懵）
        //   - 泄漏自检 liveBlocks()==0 永远不成立（判据失效）
        // 解法：在 main 开头调一次 markBaseline()，之后所有读取都减去基线。
        //
        // ⚠️ 基线【只影响读取】，不影响分配行为。
        // ⚠️ 跨基线释放（基线前分配、基线后释放）会做钳制，不让数字下溢；
        //    同时记 PreBaselineFrees 让你知道发生过这种情况。
        void markBaseline()
        {
            m_BaselineUsed = m_UsedTotal.load(std::memory_order_relaxed);
            m_BaselineAllocs = m_TotalAllocs.load(std::memory_order_relaxed);
            m_BaselineFrees = m_TotalFrees.load(std::memory_order_relaxed);
            m_BaselinePeak = m_PeakTotal.load(std::memory_order_relaxed);
            m_EarlyAllocs = m_BaselineAllocs;
            m_HasBaseline.store(true, std::memory_order_relaxed);
        }

        bool hasBaseline() const { return m_HasBaseline.load(std::memory_order_relaxed); }

        void clearBaseline() { m_HasBaseline.store(false, std::memory_order_relaxed); }

        // ── 基线相对读取（供门面 stats() 用）──
        uint64_t UsedBytesRelative() const
        {
            const uint64_t now = m_UsedTotal.load(std::memory_order_relaxed);
            if (!hasBaseline())
                return now;
            return now > m_BaselineUsed ? now - m_BaselineUsed : 0; // 钳到下溢
        }

        uint64_t PeakBytesRelative() const
        {
            const uint64_t now = m_PeakTotal.load(std::memory_order_relaxed);
            if (!hasBaseline())
                return now;
            return now > m_BaselinePeak ? now - m_BaselinePeak : 0;
        }

        uint64_t LiveBlocksRelative() const
        {
            const uint64_t allocs = m_TotalAllocs.load(std::memory_order_relaxed);
            const uint64_t frees = m_TotalFrees.load(std::memory_order_relaxed);
            if (!hasBaseline())
                return allocs - frees;
            // 跨基线释放会让 (allocs - allocs0) < (frees - frees0)，
            // 此时按 0 处理并单独计数，避免出现天文数字。
            const uint64_t dAllocs = allocs - m_BaselineAllocs;
            const uint64_t dFrees = frees - m_BaselineFrees;
            return dAllocs > dFrees ? dAllocs - dFrees : 0;
        }

        uint64_t EarlyAllocs() const { return m_EarlyAllocs; }

        uint64_t PreBaselineFrees() const
        {
            if (!hasBaseline())
                return 0;
            const uint64_t frees = m_TotalFrees.load(std::memory_order_relaxed);
            const uint64_t dAllocs = m_TotalAllocs.load(std::memory_order_relaxed) -
                                     m_BaselineAllocs;
            const uint64_t dFrees = frees - m_BaselineFrees;
            return dFrees > dAllocs ? dFrees - dAllocs : 0;
        }

        // ── 快照：拍下当前状态成 POD，之后可自由传阅 ──
        MemoryStats Snapshot() const
        {
            MemoryStats s;
            s.UsedBytes = UsedBytesRelative();
            s.PeakBytes = PeakBytesRelative();
            s.OverheadBytes = OverheadBytes();
            s.TotalAllocs = TotalAllocs();
            s.TotalFrees = TotalFrees();
            s.LiveBlocks = LiveBlocksRelative();
            s.OOMCount = OOMCount();

            s.HasBaseline = hasBaseline();
            s.EarlyAllocs = m_EarlyAllocs;
            s.PreBaselineFrees = PreBaselineFrees();
            s.BaselineUsedBytes = m_BaselineUsed;

            for (uint32_t i = 0; i < kSlots; ++i)
                s.Backends[i] = Get(static_cast<BackendType>(i));
            for (uint32_t i = 0; i < kSizeSlots; ++i)
                s.SizeClasses[i] = getSizeClass(static_cast<SizeClass>(i));
            return s;
        }

        void Reset()
        {
            for (uint32_t i = 0; i < kSlots; ++i)
            {
                Slot &slot = m_Slots[i];
                slot.UsedBytes.store(0, std::memory_order_relaxed);
                slot.CapacityBytes.store(0, std::memory_order_relaxed);
                slot.AllocCount.store(0, std::memory_order_relaxed);
                slot.FreeCount.store(0, std::memory_order_relaxed);
            }
            for (uint32_t i = 0; i < kSizeSlots; ++i)
            {
                SizeSlot &sc = m_SizeSlots[i];
                sc.AllocCount.store(0, std::memory_order_relaxed);
                sc.FreeCount.store(0, std::memory_order_relaxed);
                sc.CurrentBytes.store(0, std::memory_order_relaxed);
                sc.PeakBytes.store(0, std::memory_order_relaxed);
            }
            m_UsedTotal.store(0, std::memory_order_relaxed);
            m_PeakTotal.store(0, std::memory_order_relaxed);
            m_Overhead.store(0, std::memory_order_relaxed);
            m_TotalAllocs.store(0, std::memory_order_relaxed);
            m_TotalFrees.store(0, std::memory_order_relaxed);
            m_OOMCount.store(0, std::memory_order_relaxed);
            m_HasBaseline.store(false, std::memory_order_relaxed);
            m_EarlyAllocs = 0;
            m_BaselineUsed = 0;
            m_BaselinePeak = 0;
            m_BaselineAllocs = 0;
            m_BaselineFrees = 0;
        }

        std::string ToString() const
        {
            std::ostringstream oss;
            oss << "=== Memory Stats ===\n";
            if (hasBaseline())
            {
                oss << "(baseline applied; early allocs before baseline = "
                    << m_EarlyAllocs << ")\n";
            }
            oss << "Used:     " << UsedBytesRelative() << " bytes\n";
            oss << "Peak:     " << PeakBytesRelative() << " bytes\n";
            oss << "Overhead: " << OverheadBytes() << " bytes\n";
            oss << "Allocs:   " << TotalAllocs() << "  Frees: " << TotalFrees() << "\n";
            oss << "Live:     " << LiveBlocksRelative() << " blocks\n";
            if (hasBaseline() && PreBaselineFrees() > 0)
                oss << "PreBaselineFrees: " << PreBaselineFrees() << "\n";
            oss << "OOM:      " << OOMCount() << "\n";

            oss << "-- per backend --\n";
            const BackendType kinds[] = {BackendType::Default, BackendType::Crt,
                                         BackendType::Slab};
            for (BackendType t : kinds)
            {
                const BackendStats s = Get(t);
                if (s.AllocCount == 0 && s.CapacityBytes == 0)
                    continue;
                oss << "  " << BackendName(t) << ": used=" << s.UsedBytes
                    << " cap=" << s.CapacityBytes
                    << " alloc=" << s.AllocCount << " free=" << s.FreeCount
                    << " live=" << s.LiveCount << "\n";
            }

            // 大小档：定策略的数据依据（看程序到底在分配什么尺寸）
            oss << "-- by size class (alloc count) --\n";
            for (uint32_t i = 0; i < kSizeSlots; ++i)
            {
                const SizeClassStats sc = getSizeClass(static_cast<SizeClass>(i));
                if (sc.AllocCount == 0)
                    continue;
                oss << "  " << SizeClassName(sc.Class) << ": alloc=" << sc.AllocCount
                    << " free=" << sc.FreeCount
                    << " cur=" << sc.CurrentBytes << " peak=" << sc.PeakBytes << "\n";
            }
            oss << "=====================\n";
            return oss.str();
        }

    private:
        // 槽位按 BackendType 大小固定开。用固定数组而非 vector，
        // 保证统计器本身零堆分配 —— 它要在自举期就能用。
        static constexpr uint32_t kSlots = static_cast<uint32_t>(BackendType::Count);
        static constexpr uint32_t kSizeSlots = static_cast<uint32_t>(SizeClass::Count);

        // ⚠️ Slot 是嵌套类型，所以下面的访问器【不能】也叫 Slot()：
        //    类作用域内 Slot 会先解析成函数名 → "Slot does not name a type"。
        //    故访问器取名 slotOf。
        struct Slot
        {
            std::atomic<uint64_t> UsedBytes{0};
            std::atomic<uint64_t> CapacityBytes{0};
            std::atomic<uint64_t> AllocCount{0};
            std::atomic<uint64_t> FreeCount{0};
        };

        // 大小档计数（同上，访问器叫 sizeSlotOf 避免同名冲突）
        struct SizeSlot
        {
            std::atomic<uint64_t> AllocCount{0};
            std::atomic<uint64_t> FreeCount{0};
            std::atomic<uint64_t> CurrentBytes{0};
            std::atomic<uint64_t> PeakBytes{0};
        };

        static uint32_t Idx(BackendType t)
        {
            const uint32_t i = static_cast<uint32_t>(t);
            return i < kSlots ? i : 0;
        }

        static uint32_t SizeIdx(SizeClass c)
        {
            const uint32_t i = static_cast<uint32_t>(c);
            return i < kSizeSlots ? i : 0;
        }

        Slot &slotOf(BackendType t) { return m_Slots[Idx(t)]; }
        const Slot &slotOf(BackendType t) const { return m_Slots[Idx(t)]; }

        SizeSlot &sizeSlotOf(SizeClass c) { return m_SizeSlots[SizeIdx(c)]; }
        const SizeSlot &sizeSlotOf(SizeClass c) const { return m_SizeSlots[SizeIdx(c)]; }

        SizeClassStats getSizeClass(SizeClass c) const
        {
            SizeClassStats s;
            const SizeSlot &slot = sizeSlotOf(c);
            s.Class = c;
            s.AllocCount = slot.AllocCount.load(std::memory_order_relaxed);
            s.FreeCount = slot.FreeCount.load(std::memory_order_relaxed);
            s.CurrentBytes = slot.CurrentBytes.load(std::memory_order_relaxed);
            s.PeakBytes = slot.PeakBytes.load(std::memory_order_relaxed);
            return s;
        }

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
        uint64_t m_EarlyAllocs = 0;
        uint64_t m_BaselineUsed = 0;
        uint64_t m_BaselinePeak = 0;
        uint64_t m_BaselineAllocs = 0;
        uint64_t m_BaselineFrees = 0;
    };

} // namespace X_Y
