#include "BufferPool.h"
#include <cstdlib>
#include <cstring>

namespace X_Y {

BufferPool::BufferPool(uint64_t blockSize, uint32_t maxBlocks)
    : m_BlockSize(blockSize)
    , m_MaxBlocks(maxBlocks)
{
}

BufferPool::~BufferPool()
{
    for (auto& block : m_Blocks)
    {
        if (block.Memory)
            std::free(block.Memory);
    }
    m_Blocks.clear();
    m_FreeList.clear();
}

BufferPool::Block* BufferPool::AllocNewBlock()
{
    if (m_MaxBlocks > 0 && m_TotalBlocks >= m_MaxBlocks)
        return nullptr;

    Block block;
    block.Memory = static_cast<uint8_t*>(std::malloc(m_BlockSize));
    if (!block.Memory)
        return nullptr;

    m_Blocks.push_back(block);
    m_TotalBlocks++;
    return &m_Blocks.back();
}

Buffer BufferPool::Allocate(uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    uint32_t blockIdx;
    if (!m_FreeList.empty())
    {
        blockIdx = m_FreeList.back();
        m_FreeList.pop_back();
    }
    else
    {
        Block* newBlock = AllocNewBlock();
        if (!newBlock)
            return Buffer();

        blockIdx = static_cast<uint32_t>(m_Blocks.size() - 1);
    }

    Block& block = m_Blocks[blockIdx];
    m_UsedBytes += m_BlockSize;

    Buffer result;
    result.Data = block.Memory;
    result.Capacity = m_BlockSize;
    result.Size = size > m_BlockSize ? m_BlockSize : size;
    result.bFreeInstead = false;

    // 关键：lambda 捕获 this，析构时自动归还到本 Pool
    // mutable 因为 lambda 体不修改捕获，只是转发
    result.Deleter = [this](uint8_t* data, uint64_t, uint64_t) {
        this->DeallocateRaw(data);
    };

    return result;
}

void BufferPool::DeallocateRaw(uint8_t* data)
{
    // 内部方法：只找块索引，不操作 Buffer 对象
    // 由 Deleter 回调调用，此时 mutex 已由外部 Allocate 释放？
    // 不对——Deleter 是从 Buffer 析构中调用的，析构可能发生在任意线程
    // 所以需要加锁

    std::lock_guard<std::mutex> lock(m_Mutex);

    for (uint32_t i = 0; i < m_TotalBlocks; i++)
    {
        if (m_Blocks[i].Memory == data)
        {
            m_FreeList.push_back(i);
            m_UsedBytes -= m_BlockSize;
            return;
        }
    }
    // 没找到——说明 data 不是本池的内存，忽略（防御性）
}

void BufferPool::Deallocate(Buffer& buf)
{
    if (!buf.Data)
        return;

    DeallocateRaw(buf.Data);

    buf.Data = nullptr;
    buf.Size = 0;
    buf.Capacity = 0;
    buf.Deleter = nullptr;
}

bool BufferPool::Grow(uint32_t numBlocks)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (uint32_t i = 0; i < numBlocks; i++)
    {
        if (m_MaxBlocks > 0 && m_TotalBlocks >= m_MaxBlocks)
            return false;

        Block block;
        block.Memory = static_cast<uint8_t*>(std::malloc(m_BlockSize));
        if (!block.Memory)
            return false;

        m_Blocks.push_back(block);
        m_FreeList.push_back(m_TotalBlocks);
        m_TotalBlocks++;
    }
    return true;
}

void BufferPool::Shrink(uint32_t minBlocks)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    while (m_FreeList.size() > minBlocks)
    {
        uint32_t idx = m_FreeList.back();
        m_FreeList.pop_back();

        std::free(m_Blocks[idx].Memory);
        m_Blocks[idx].Memory = nullptr;
    }
}

} // namespace X_Y
