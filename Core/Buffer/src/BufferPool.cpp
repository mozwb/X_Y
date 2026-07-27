#include "BufferPool.h"
#include <cstdlib>
#include <cstring>

namespace X_Y {

BufferPool::BufferPool(uint64_t blockSize, uint32_t maxBlocks)
    : m_BlockSize(blockSize)
    , m_MaxBlocks(maxBlocks)
{
    // 初始不预分配，按需 Grow
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
            return Buffer();  // 分配失败

        blockIdx = static_cast<uint32_t>(m_Blocks.size() - 1);
    }

    Block& block = m_Blocks[blockIdx];
    m_UsedBytes += m_BlockSize;

    Buffer result;
    result.Data = block.Memory;
    result.Capacity = m_BlockSize;
    result.Size = size > m_BlockSize ? m_BlockSize : size;
    result.bFreeInstead = true;  // 池内存用 free 释放（Deallocate 归还，不真正 free）

    return result;
}

void BufferPool::Deallocate(Buffer& buf)
{
    if (!buf.Data)
        return;

    std::lock_guard<std::mutex> lock(m_Mutex);

    // 找到 buf.Data 对应的块索引
    for (uint32_t i = 0; i < m_TotalBlocks; i++)
    {
        if (m_Blocks[i].Memory == buf.Data)
        {
            m_FreeList.push_back(i);
            m_UsedBytes -= m_BlockSize;
            break;
        }
    }

    buf.Data = nullptr;
    buf.Size = 0;
    buf.Capacity = 0;
    buf.bFreeInstead = false;
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

    // 只释放空闲块，保留至少 minBlocks
    while (m_FreeList.size() > minBlocks)
    {
        uint32_t idx = m_FreeList.back();
        m_FreeList.pop_back();

        std::free(m_Blocks[idx].Memory);
        m_Blocks[idx].Memory = nullptr;
    }
}

} // namespace X_Y
