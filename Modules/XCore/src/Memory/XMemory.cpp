#include "Memory/XMemory.h"
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <sstream>
#include <algorithm>

namespace X_Y
{

    // ═════════════════════════════════════════════════════════════════════════════
    //  全局实例
    // ═════════════════════════════════════════════════════════════════════════════

    Memory &Memory::Instance()
    {
        static Memory inst;
        return inst;
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  析构 / 关闭
    // ═════════════════════════════════════════════════════════════════════════════

    Memory::~Memory()
    {
        // 程序结束，释放所有 chunk（不需要加锁，此时已是单线程）
        for (auto &chunk : m_AllChunks)
            std::free(reinterpret_cast<void *>(chunk.Base));
        m_AllChunks.clear();

        for (uint32_t i = 0; i < kSlabCount; i++)
        {
            auto &slab = m_Slabs[i];
            slab.Chunks.clear();
            slab.FreeList.clear();
            slab.TotalBlocks = 0;
            slab.FreeBlocks = 0;
        }
    }

    void Memory::Shutdown()
    {
        if (m_Shutdown)
            return;

        m_ShuttingDown = true;

        // 锁住所有 slab 防止并发放缩
        for (uint32_t i = 0; i < kSlabCount; i++)
            m_Slabs[i].Mutex.lock();

        {
            std::unique_lock lock(m_ChunksMutex);
            for (auto &chunk : m_AllChunks)
                std::free(reinterpret_cast<void *>(chunk.Base));
            m_AllChunks.clear();
        }

        for (uint32_t i = 0; i < kSlabCount; i++)
        {
            auto &slab = m_Slabs[i];
            slab.Chunks.clear();
            slab.FreeList.clear();
            slab.TotalBlocks = 0;
            slab.FreeBlocks = 0;
            slab.Mutex.unlock();
        }

        m_Shutdown = true;
        m_ShuttingDown = false;
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  配置
    // ═════════════════════════════════════════════════════════════════════════════

    void Memory::SetMaxBytes(uint64_t bytes)
    {
        s_MaxBytes = bytes;
    }

    uint64_t Memory::MaxBytes() const
    {
        return s_MaxBytes;
    }

    void Memory::SetOOMAction(OOMAction action)
    {
        s_OOMAction = action;
    }

    OOMAction Memory::GetOOMAction() const
    {
        return s_OOMAction;
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  预算
    // ═════════════════════════════════════════════════════════════════════════════

    uint64_t Memory::UsedBytes() const
    {
        return m_UsedBytes.load(std::memory_order_relaxed);
    }

    uint64_t Memory::UsedCapacity() const
    {
        return m_UsedCapacity.load(std::memory_order_relaxed);
    }

    bool Memory::UnderBudget() const
    {
        return m_UsedCapacity.load(std::memory_order_relaxed) < s_MaxBytes;
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  内部工具
    // ═════════════════════════════════════════════════════════════════════════════

    uint32_t Memory::FindSlabIndex(uint64_t size) const
    {
        // 找到精确匹配的最小 slab 索引
        uint32_t exact = 0;
        for (; exact < kSlabCount; exact++)
            if (size <= kSlabSizes[exact])
                break;

        if (exact >= kSlabCount)
            return kSlabCount - 1;

        // 优先从已有 chunk 且有空闲块的更大 slab 借用
        // 这样能让大 chunk 优先被使用、优先被回收，减少 chunk 总量
        for (uint32_t i = exact; i < kSlabCount; i++)
        {
            std::lock_guard lock(m_Slabs[i].Mutex);
            if (m_Slabs[i].FreeBlocks > 0)
                return i;
        }

        // 都没有空闲块 → 回到精确匹配的 slab（触发 AddChunk）
        return exact;
    }

    void Memory::AddChunk(uint32_t slabIdx)
    {
        uint64_t slabSize = kSlabSizes[slabIdx];
        uint64_t chunkSize = kChunkSize;

        // 确保 chunk 大小是 slab 大小的整数倍
        // 对于大 slab（如 64KB），chunk 就是 64KB，刚好 1 块
        if (chunkSize % slabSize != 0)
            chunkSize = slabSize;

        void *mem = std::malloc(chunkSize);
        if (!mem)
        {
            // malloc 失败 -> 按 OOMAction 处理（在 AddChunk 调用方处理）
            return;
        }

        std::memset(mem, 0, chunkSize);

        uint64_t blockCount = chunkSize / slabSize;

        ChunkInfo info;
        info.Base = reinterpret_cast<uintptr_t>(mem);
        info.Size = chunkSize;
        info.SlabIndex = slabIdx;
        info.FreeBlocks = static_cast<uint32_t>(blockCount);

        // 注册到全局
        {
            std::unique_lock lock(m_ChunksMutex);
            m_AllChunks.push_back(info);
            // 保持按基址排序
            std::sort(m_AllChunks.begin(), m_AllChunks.end(),
                      [](const ChunkInfo &a, const ChunkInfo &b)
                      {
                          return a.Base < b.Base;
                      });
        }

        // 注册到 slab
        auto &slab = m_Slabs[slabIdx];
        slab.Chunks.push_back(info);
        slab.TotalBlocks += static_cast<uint32_t>(blockCount);
        slab.FreeBlocks += static_cast<uint32_t>(blockCount);

        // 直接记录 block 地址，避免 Chunk 删除后全局索引失效
        for (uint64_t i = 0; i < blockCount; i++)
            slab.FreeList.push_back(info.Base + i * slabSize);
    }

    bool Memory::FindChunkByAddr(uintptr_t addr, ChunkInfo &out) const
    {
        std::shared_lock lock(m_ChunksMutex);

        if (m_AllChunks.empty())
            return false;

        // 二分查找：找到最后一个 Base <= addr 的 chunk
        auto it = std::upper_bound(m_AllChunks.begin(), m_AllChunks.end(), addr,
                                   [](uintptr_t a, const ChunkInfo &c)
                                   {
                                       return a < c.Base;
                                   });

        if (it == m_AllChunks.begin())
            return false;

        --it;
        if (addr >= it->Base && addr < it->Base + it->Size)
        {
            out = *it;
            return true;
        }

        return false;
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  Slab 分配 / 释放
    // ═════════════════════════════════════════════════════════════════════════════

    void *Memory::AllocFromSlab(uint32_t slabIdx)
    {
        auto &slab = m_Slabs[slabIdx];
        uint64_t slabSize = kSlabSizes[slabIdx];

        // 调用者已经持有 slab 锁。FreeList 中保存的是实际 block 地址，
        // 因此 Chunks 增删不会让待分配的地址索引失效。
        if (slab.FreeBlocks > 0)
            goto alloc_block;

        // 没有空闲 block：先检查预算，再申请一个 chunk，并把它切成固定大小的 block。
        {
            uint64_t newChunkCost = kChunkSize;
            if (m_UsedCapacity.load(std::memory_order_relaxed) + newChunkCost > s_MaxBytes)
            {
                switch (s_OOMAction)
                {
                case OOMAction::Abort:
                    std::fprintf(stderr, "[Memory] OOM: exceeded budget of %llu bytes\n",
                                 (unsigned long long)s_MaxBytes);
                    std::terminate();
                    return nullptr;
                case OOMAction::ReturnNull:
                    m_OOMCount.fetch_add(1, std::memory_order_relaxed);
                    return nullptr;
                case OOMAction::Expand:
                    m_OOMCount.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            }

            AddChunk(slabIdx);
            if (slab.FreeBlocks == 0)
                return nullptr; // malloc 失败
        }

    alloc_block:
        // 从 free list 取一块
        {
            uintptr_t blockAddr = slab.FreeList.back();
            slab.FreeList.pop_back();
            slab.FreeBlocks--;

            for (auto &chunk : slab.Chunks)
            {
                if (blockAddr >= chunk.Base && blockAddr < chunk.Base + chunk.Size)
                {
                    chunk.FreeBlocks--;
                    void *ptr = reinterpret_cast<void *>(blockAddr);

                    m_UsedBytes.fetch_add(slabSize, std::memory_order_relaxed);
                    m_UsedCapacity.fetch_add(slabSize, std::memory_order_relaxed);

                    uint64_t cap = m_UsedCapacity.load(std::memory_order_relaxed);
                    uint64_t peak = m_PeakBytes.load(std::memory_order_relaxed);
                    while (cap > peak && !m_PeakBytes.compare_exchange_weak(peak, cap))
                        ;

                    m_TotalAllocs.fetch_add(1, std::memory_order_relaxed);
                    return ptr;
                }
            }

            std::fprintf(stderr, "[Memory] internal error: block index out of range\n");
            std::terminate();
            return nullptr;
        }
    }

    void Memory::FreeToSlab(void *ptr, const ChunkInfo &chunk, uint32_t slabIdx)
    {
        auto &slab = m_Slabs[slabIdx];
        uint64_t slabSize = kSlabSizes[slabIdx];

        // 调用者已经持有 slab 锁。先把 block 归还给 slab 和所属 chunk。
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        slab.FreeList.push_back(addr);
        slab.FreeBlocks++;

        ChunkInfo *ownedChunk = nullptr;
        for (auto &candidate : slab.Chunks)
        {
            if (candidate.Base == chunk.Base)
            {
                candidate.FreeBlocks++;
                ownedChunk = &candidate;
                break;
            }
        }

        m_UsedBytes.fetch_sub(slabSize, std::memory_order_relaxed);
        m_UsedCapacity.fetch_sub(slabSize, std::memory_order_relaxed);
        m_TotalFrees.fetch_add(1, std::memory_order_relaxed);

        // 只有当前 chunk 自己完全空闲时才能回收，不能使用 slab.FreeBlocks 判断。
        if (!ownedChunk || ownedChunk->FreeBlocks != ownedChunk->BlockCount() || slab.Chunks.size() <= 1)
            return;

        const uintptr_t base = ownedChunk->Base;
        const uintptr_t end = base + ownedChunk->Size;
        slab.FreeList.erase(
            std::remove_if(slab.FreeList.begin(), slab.FreeList.end(),
                           [base, end](uintptr_t freeAddr)
                           { return freeAddr >= base && freeAddr < end; }),
            slab.FreeList.end());

        {
            std::unique_lock lock(m_ChunksMutex);
            m_AllChunks.erase(
                std::remove_if(m_AllChunks.begin(), m_AllChunks.end(),
                               [base](const ChunkInfo &item)
                               { return item.Base == base; }),
                m_AllChunks.end());
        }

        std::free(reinterpret_cast<void *>(base));
        slab.TotalBlocks -= static_cast<uint32_t>(ownedChunk->BlockCount());
        slab.FreeBlocks -= static_cast<uint32_t>(ownedChunk->BlockCount());
        slab.Chunks.erase(
            std::remove_if(slab.Chunks.begin(), slab.Chunks.end(),
                           [base](const ChunkInfo &item)
                           { return item.Base == base; }),
            slab.Chunks.end());
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  核心分配 / 释放
    // ═════════════════════════════════════════════════════════════════════════════

    void *Memory::Alloc(uint64_t size)
    {
        if (size == 0 || m_ShuttingDown)
            return nullptr;

        if (size <= kMaxSlabSize)
        {
            // 小对象走 slab：多个固定大小 block 共享一个 64KB chunk。
            uint32_t slabIdx = FindSlabIndex(size);
            auto &slab = m_Slabs[slabIdx];

            std::lock_guard lock(slab.Mutex);
            return AllocFromSlab(slabIdx);
        }

        // 大于最大 slab 的对象不切块，直接走 malloc；释放时同样走 std::free。
        uint64_t newCost = size;
        if (m_UsedCapacity.load(std::memory_order_relaxed) + newCost > s_MaxBytes)
        {
            switch (s_OOMAction)
            {
            case OOMAction::Abort:
                std::fprintf(stderr, "[Memory] OOM: exceeded budget of %llu bytes\n",
                             (unsigned long long)s_MaxBytes);
                std::terminate();
                break;
            case OOMAction::ReturnNull:
                m_OOMCount.fetch_add(1, std::memory_order_relaxed);
                return nullptr;
            case OOMAction::Expand:
                m_OOMCount.fetch_add(1, std::memory_order_relaxed);
                break;
            }
        }

        void *ptr = std::malloc(size);
        if (!ptr)
            return nullptr;

        std::memset(ptr, 0, size);

        m_UsedBytes.fetch_add(size, std::memory_order_relaxed);
        m_UsedCapacity.fetch_add(size, std::memory_order_relaxed);
        uint64_t cap = m_UsedCapacity.load(std::memory_order_relaxed);
        uint64_t peak = m_PeakBytes.load(std::memory_order_relaxed);
        while (cap > peak && !m_PeakBytes.compare_exchange_weak(peak, cap))
            ;
        m_TotalAllocs.fetch_add(1, std::memory_order_relaxed);
        m_MallocFallbacks.fetch_add(1, std::memory_order_relaxed);

        return ptr;
    }

    void Memory::Free(void *ptr)
    {
        if (!ptr || m_ShuttingDown)
            return;

        // 先通过全局 chunk 索引判断 ptr 属于 slab 还是独立 malloc。
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        ChunkInfo chunk;

        if (FindChunkByAddr(addr, chunk))
        {
            // slab 内存：归还 block，必要时回收完全空闲的 chunk。
            uint32_t slabIdx = chunk.SlabIndex;
            auto &slab = m_Slabs[slabIdx];

            std::lock_guard lock(slab.Mutex);
            FreeToSlab(ptr, chunk, slabIdx);
        }
        else
        {
            // 非 slab 内存：这是 >64KB 的独立 malloc，直接交给 std::free。
            uint64_t size = 0; // 无法知道 malloc 的大小，只减计数
            // 理想情况应该记录大小，但 std::malloc 不提供
            // 这里用近似值处理
            m_TotalFrees.fetch_add(1, std::memory_order_relaxed);
            std::free(ptr);
        }
    }

    // ═════════════════════════════════════════════════════════════════════════════
    //  统计
    // ═════════════════════════════════════════════════════════════════════════════

    MemoryStats Memory::GetStats() const
    {
        MemoryStats stats;

        stats.UsedBytes = m_UsedBytes.load(std::memory_order_relaxed);
        stats.UsedCapacity = m_UsedCapacity.load(std::memory_order_relaxed);
        stats.PeakBytes = m_PeakBytes.load(std::memory_order_relaxed);

        // TotalRawBytes = 所有 chunk 大小之和
        {
            std::shared_lock lock(m_ChunksMutex);
            for (const auto &chunk : m_AllChunks)
                stats.TotalRawBytes += chunk.Size;
        }

        stats.TotalAllocs = m_TotalAllocs.load(std::memory_order_relaxed);
        stats.TotalFrees = m_TotalFrees.load(std::memory_order_relaxed);
        stats.MallocFallbacks = m_MallocFallbacks.load(std::memory_order_relaxed);
        stats.OOMCount = m_OOMCount.load(std::memory_order_relaxed);

        for (uint32_t i = 0; i < kSlabCount; i++)
        {
            const auto &slab = m_Slabs[i];
            std::lock_guard lock(slab.Mutex);

            stats.Slabs[i].SlabSize = kSlabSizes[i];
            stats.Slabs[i].UsedBlocks = slab.TotalBlocks - slab.FreeBlocks;
            stats.Slabs[i].FreeBlocks = slab.FreeBlocks;
            stats.Slabs[i].TotalBlocks = slab.TotalBlocks;
            stats.Slabs[i].ChunkCount = static_cast<uint32_t>(slab.Chunks.size());
        }

        return stats;
    }

    std::string Memory::ToString() const
    {
        MemoryStats s = GetStats();

        std::ostringstream oss;
        oss << "=== Memory Stats ===\n";
        oss << "Budget: " << s_MaxBytes << " bytes\n";
        oss << "Used:   " << s.UsedBytes << " bytes (data) / "
            << s.UsedCapacity << " bytes (capacity)\n";
        oss << "Peak:   " << s.PeakBytes << " bytes\n";
        oss << "Raw:    " << s.TotalRawBytes << " bytes (from OS)\n";
        oss << "Allocs: " << s.TotalAllocs << " / Frees: " << s.TotalFrees << "\n";
        oss << "Malloc fallbacks: " << s.MallocFallbacks << "\n";
        oss << "OOM events: " << s.OOMCount << "\n";
        oss << "OOM action: ";
        switch (s_OOMAction)
        {
        case OOMAction::Abort:
            oss << "Abort";
            break;
        case OOMAction::ReturnNull:
            oss << "ReturnNull";
            break;
        case OOMAction::Expand:
            oss << "Expand";
            break;
        }
        oss << "\n\n";

        oss << "Slab utilization:\n";
        for (uint32_t i = 0; i < kSlabCount; i++)
        {
            const auto &si = s.Slabs[i];
            oss << "  " << si.SlabSize << "B: "
                << (si.TotalBlocks - si.FreeBlocks) << "/" << si.TotalBlocks
                << " blocks used, "
                << si.ChunkCount << " chunks\n";
        }

        oss << "========================\n";
        return oss.str();
    }

}
// namespace X_Y
