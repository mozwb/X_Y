#pragma once
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <atomic>
#include <cstdio>
#include <new>

namespace X_Y {

// ═════════════════════════════════════════════════════════════════════════════
//  Memory — 统一内存管理器
//
//  核心思想：
//    - 预先拿大块内存，切成固定尺寸 slab，按需零售
//    - 每 slab 一把锁，减少跨线程冲突
//    - 大块（>64KB）直接 malloc，但统计入账
//    - 所有分配释放统一入口，全程序内存可见可控
//
//  Slab 家族（固定 8 级）：
//    64B | 128B | 256B | 512B | 1KB | 4KB | 16KB | 64KB
//
//  Chunk：
//    每个 chunk 64KB 连续内存，内部全部切成同一尺寸 slab
//    用完自动申请新 chunk（惰性扩展）
//
//  锁策略：
//    每 slab 一把 std::mutex，保护该 slab 的 free list
//    跨 slab 分配完全并行
//    全局 chunk 索引写时加锁，读时 shared_mutex（很少写）
// ═════════════════════════════════════════════════════════════════════════════

// ── 超限处理策略 ──

enum class OOMAction {
    Abort,       // 开发期：超限直接 terminate，尽早暴露问题
    ReturnNull,  // 发布版：超限返回 nullptr，调用方自己处理
    Expand,      // 调试期：突破预算继续分配，在统计中标记为超限
};

// ── 统计（只读快照） ──

struct MemoryStats {
    uint64_t UsedBytes = 0;
    uint64_t UsedCapacity = 0;
    uint64_t PeakBytes = 0;
    uint64_t TotalRawBytes = 0;
    uint64_t TotalAllocs = 0;
    uint64_t TotalFrees = 0;
    uint64_t MallocFallbacks = 0;
    uint64_t OOMCount = 0;

    struct SlabInfo {
        uint64_t SlabSize = 0;
        uint32_t UsedBlocks = 0;
        uint32_t FreeBlocks = 0;
        uint32_t TotalBlocks = 0;
        uint32_t ChunkCount = 0;
    };
    SlabInfo Slabs[8];
};

// ── Memory — 全局唯一内存管理器 ────────────────────────────────────────────


class Memory {
public:
    static Memory& Instance();

    void Shutdown();

    // ── 配置 ──
    
    void SetMaxBytes(uint64_t bytes);
    uint64_t MaxBytes() const;

    void SetOOMAction(OOMAction action);
    OOMAction GetOOMAction() const;

    // ── 核心分配 ──
    
    void* Alloc(uint64_t size);
    void  Free(void* ptr);

    template<typename T, typename... Args>
    T* AllocT(Args&&... args)
    {
        void* mem = Alloc(sizeof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void FreeT(T* ptr)
    {
        if (!ptr) return;
        ptr->~T();
        Free(ptr);
    }

    // ── 统计 ──
    MemoryStats GetStats() const;
    std::string ToString() const;

    uint64_t UsedBytes() const;
    uint64_t UsedCapacity() const;
    bool     UnderBudget() const;

private:
    Memory() = default;
    ~Memory();
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    static constexpr uint64_t kSlabSizes[8] = {
        64, 128, 256, 512, 1024, 4096, 16384, 65536
    };
    static constexpr uint32_t kSlabCount = 8;
    static constexpr uint64_t kChunkSize = 64 * 1024;
    static constexpr uint64_t kMaxSlabSize = 65536;

    struct ChunkInfo {
        uintptr_t Base = 0;
        uint64_t  Size = 0;
        uint32_t  SlabIndex = 0;

        uint64_t BlockCount() const {
            return Size / kSlabSizes[SlabIndex];
        }
    };

    struct SlabManager {
        mutable std::mutex   Mutex;
        std::vector<uint64_t> FreeList;
        std::vector<ChunkInfo> Chunks;
        uint32_t TotalBlocks = 0;
        uint32_t FreeBlocks = 0;
    };

    uint32_t FindSlabIndex(uint64_t size) const;
    void*    AllocFromSlab(uint32_t slabIdx);
    void     FreeToSlab(void* ptr, const ChunkInfo& chunk, uint32_t slabIdx);
    void     AddChunk(uint32_t slabIdx);
    const ChunkInfo* FindChunkByAddr(uintptr_t addr) const;

    // ── 成员 ──
    
    bool m_Shutdown = false;
    bool m_ShuttingDown = false;

    // 配置（inline static，全局唯一）
    
    inline static uint64_t  s_MaxBytes = 256 * 1024 * 1024;
    inline static OOMAction s_OOMAction = OOMAction::Abort;

    SlabManager m_Slabs[8];

    mutable std::shared_mutex  m_ChunksMutex;
    std::vector<ChunkInfo>     m_AllChunks;

    std::atomic<uint64_t> m_UsedBytes{0};
    std::atomic<uint64_t> m_UsedCapacity{0};
    std::atomic<uint64_t> m_PeakBytes{0};
    std::atomic<uint64_t> m_TotalAllocs{0};
    std::atomic<uint64_t> m_TotalFrees{0};
    std::atomic<uint64_t> m_MallocFallbacks{0};
    std::atomic<uint64_t> m_OOMCount{0};
};

} 
// namespace X_Y
