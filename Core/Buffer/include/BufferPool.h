#pragma once
#include "Buffer.h"
#include <vector>
#include <mutex>

namespace X_Y {

// ── BufferPool ──
// 通用内存池：预分配固定大小的块，分配/回收复用内存
// 线程安全（internal mutex）
//
// 核心设计：Pool::Allocate 返回的 Buffer 设自定义 Deleter（lambda 捕获 this），
//           析构时自动调用 Pool::Deallocate 归还，无需用户操心
//
// 使用方式：
//   BufferPool pool(65536);
//   Buffer buf = pool.Allocate(256);
//   // buf 超出作用域时自动归还到池，无需手动操作
//
// 注意：从 Pool 分配的 Buffer 如果调 Reserve/Ensure/Allocate 扩容，
//       会先 Release（归还池内存），再 new 新内存。
//       这是安全的行为——"池内存不够用了就切到自管"。

class BufferPool {
public:
    // blockSize: 每个内存块的大小（字节）
    // maxBlocks: 池最大块数（0 = 不限，默认 0）
    explicit BufferPool(uint64_t blockSize = 65536, uint32_t maxBlocks = 0);

    ~BufferPool();

    // ── 分配 ──
    // 从池中分配一块 Buffer，返回的 Buffer 容量为 blockSize
    // 析构时自动归还到池
    Buffer Allocate(uint64_t size);

    // ── 手动回收 ──
    // 提前归还 Buffer 到池，调用后 buf 清空
    void Deallocate(Buffer& buf);

    // ── 扩容/缩容 ──
    bool Grow(uint32_t numBlocks);
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
    void DeallocateRaw(uint8_t* data);

    uint64_t m_BlockSize;
    uint32_t m_MaxBlocks;
    uint32_t m_TotalBlocks = 0;
    uint64_t m_UsedBytes = 0;

    std::vector<Block> m_Blocks;
    std::vector<uint32_t> m_FreeList;

    mutable std::mutex m_Mutex;

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
};

} // namespace X_Y
