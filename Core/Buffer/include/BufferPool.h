#pragma once
#include "Buffer.h"
#include <vector>
#include <mutex>

namespace X_Y {

// ── BufferPool ──
// 通用内存池：预分配固定大小的块，分配/回收复用内存
// 线程安全（internal mutex）
//
// 典型用途：网络收发、文件缓存、编辑器撤销栈等需要频繁分配固定大小内存的场景
//
// 使用方式：
//   BufferPool pool(65536);          // 每块 64KB
//   Buffer buf = pool.Allocate(256); // 从池中取一块，容量至少 256 字节
//   pool.Deallocate(buf);            // 归还（内存回到池中）

class BufferPool {
public:
    // blockSize: 每个内存块的大小（字节）
    // maxBlocks: 池最大块数（0 = 不限，默认 0）
    explicit BufferPool(uint64_t blockSize = 65536, uint32_t maxBlocks = 0);

    ~BufferPool();

    // ── 分配 ──
    // 从池中分配一块 Buffer，容量至少为 size（按块大小对齐）
    // 池空时自动 new 一块新内存
    Buffer Allocate(uint64_t size);

    // ── 回收 ──
    // 归还 Buffer，内存回到池中供复用
    // 注意：调用后 buf.Data 置 nullptr，不要继续使用
    void Deallocate(Buffer& buf);

    // ── 扩容 ──
    // 预先分配 numBlocks 块
    bool Grow(uint32_t numBlocks);

    // ── 缩容 ──
    // 释放空闲块，保留至少 minBlocks 块
    void Shrink(uint32_t minBlocks = 0);

    // ── 查询 ──
    uint64_t BlockSize() const { return m_BlockSize; }
    uint32_t TotalBlocks() const { return m_TotalBlocks; }
    uint32_t FreeBlocks() const { return m_FreeList.size(); }
    uint64_t UsedBytes() const { return m_UsedBytes; }

private:
    struct Block {
        uint8_t* Memory = nullptr;
    };

    Block* AllocNewBlock();

    uint64_t m_BlockSize;
    uint32_t m_MaxBlocks;
    uint32_t m_TotalBlocks = 0;
    uint64_t m_UsedBytes = 0;

    // 所有已分配的块
    std::vector<Block> m_Blocks;
    // 空闲块索引栈
    std::vector<uint32_t> m_FreeList;

    mutable std::mutex m_Mutex;

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
};

} // namespace X_Y
