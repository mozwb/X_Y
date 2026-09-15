#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  MemoryStats.h — 内存统计（第 2 步：统计独立成模块）
//
//  职责边界（重要）：
//    本文件【只记账】，不认识后端、不认识分配、不认识对象。
//    它只暴露几个动作：
//      onAllocate(size, backend)   —— 有人分配了，记一笔
//      onDeallocate(size, backend) —— 有人释放了，销一笔
//    内存从哪来、怎么还，全是 MemoryBackend.h 的事。
//
//  三个必须分清的数字（别混成一个，这是验收泄漏的关键）：
//    UsedBytes  用户用量：真正 allocate 出去的字节。
//               ★ 验收"有没有泄漏"看它回到 0，不看 capacity。
//    Capacity   后端向 OS 要来的总字节。slab 会缓存 chunk 不还给 OS，
//               所以 capacity 不归零是【正常的】，不是泄漏。
//    Overhead   元数据 + header 的开销。调优时看，不影响泄漏判断。
//
//  按后端分开记（不是只有一个总数）：
//    这样才能回答"CRT 用了多少 / Slab 用了多少"，
//    进而判断"某个类该不该切到 slab"。
//
//  命名区分：
//    MemoryCounter —— 活的计数器（原子内部状态）
//    MemoryStats   —— 只读快照（POD，可安全传阅）
// ═════════════════════════════════════════════════════════════════════════════

#include "MemoryBackend.h"

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
        BackendStats Backends[static_cast<uint32_t>(BackendType::Count)];

        BackendStats Get(BackendType t) const
        {
            const uint32_t i = static_cast<uint32_t>(t);
            return i < static_cast<uint32_t>(BackendType::Count) ? Backends[i]
                                                                 : BackendStats{};
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

        // ── 快照：拍下当前状态成 POD，之后可自由传阅 ──
        MemoryStats Snapshot() const
        {
            MemoryStats s;
            s.UsedBytes = UsedBytes();
            s.PeakBytes = PeakBytes();
            s.OverheadBytes = OverheadBytes();
            s.TotalAllocs = TotalAllocs();
            s.TotalFrees = TotalFrees();
            s.LiveBlocks = LiveBlocks();
            s.OOMCount = OOMCount();
            for (uint32_t i = 0; i < kSlots; ++i)
                s.Backends[i] = Get(static_cast<BackendType>(i));
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
            m_UsedTotal.store(0, std::memory_order_relaxed);
            m_PeakTotal.store(0, std::memory_order_relaxed);
            m_Overhead.store(0, std::memory_order_relaxed);
            m_TotalAllocs.store(0, std::memory_order_relaxed);
            m_TotalFrees.store(0, std::memory_order_relaxed);
            m_OOMCount.store(0, std::memory_order_relaxed);
        }

        std::string ToString() const
        {
            std::ostringstream oss;
            oss << "=== Memory Stats ===\n";
            oss << "Used:     " << UsedBytes() << " bytes\n";
            oss << "Peak:     " << PeakBytes() << " bytes\n";
            oss << "Overhead: " << OverheadBytes() << " bytes\n";
            oss << "Allocs:   " << TotalAllocs() << "  Frees: " << TotalFrees() << "\n";
            oss << "Live:     " << LiveBlocks() << " blocks\n";
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
            oss << "=====================\n";
            return oss.str();
        }

    private:
        // 槽位按 BackendType 大小固定开。用固定数组而非 vector，
        // 保证统计器本身零堆分配 —— 它要在自举期就能用。
        static constexpr uint32_t kSlots = static_cast<uint32_t>(BackendType::Count);

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

        static uint32_t Idx(BackendType t)
        {
            const uint32_t i = static_cast<uint32_t>(t);
            return i < kSlots ? i : 0;
        }

        Slot &slotOf(BackendType t) { return m_Slots[Idx(t)]; }
        const Slot &slotOf(BackendType t) const { return m_Slots[Idx(t)]; }

        Slot m_Slots[kSlots];

        std::atomic<uint64_t> m_UsedTotal{0};
        std::atomic<uint64_t> m_PeakTotal{0};
        std::atomic<uint64_t> m_Overhead{0};
        std::atomic<uint64_t> m_TotalAllocs{0};
        std::atomic<uint64_t> m_TotalFrees{0};
        std::atomic<uint64_t> m_OOMCount{0};
    };

} // namespace X_Y
