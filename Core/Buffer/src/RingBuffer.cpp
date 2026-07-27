#include "RingBuffer.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace X_Y {

RingBuffer::RingBuffer(uint64_t capacity)
    : m_Capacity(capacity)
{
    m_Buffer = static_cast<uint8_t*>(std::malloc(capacity));
    std::memset(m_Buffer, 0, capacity);
}

RingBuffer::~RingBuffer()
{
    std::free(m_Buffer);
}

uint64_t RingBuffer::AllocBlock(uint64_t dataLen)
{
    // 总需求：BlockInfo + dataLen
    uint64_t needed = sizeof(BlockInfo) + dataLen;

    // 如果需求超过总容量，截断到容量
    if (needed > m_Capacity)
        dataLen = m_Capacity - sizeof(BlockInfo);
    needed = sizeof(BlockInfo) + dataLen;

    uint64_t startPos = m_WriteIndex;

    // 检查从 startPos 到结尾是否够放
    if (startPos + needed > m_Capacity)
    {
        // 不够 → 标记剩余部分为"空洞"（size=0 表示无效）
        // 清零剩余部分（可选，仅用于调试）
        std::memset(m_Buffer + startPos, 0, m_Capacity - startPos);
        startPos = 0;
    }

    // 如果起始位置有有效数据，覆盖之（环形覆盖）
    // 更新 ReadIndex
    if (m_ReadIndex == -1)
    {
        m_ReadIndex = static_cast<int64_t>(startPos);
    }

    // 检查是否会覆盖到 m_ReadIndex 指向的数据
    // 简单策略：如果覆盖到了，调整 ReadIndex 跳过被覆盖的块
    // （这块比较精细，简化处理：如果环形满了，ReadIndex 随 WriteIndex 移动）
    if (m_TotalBlocks > 0)
    {
        // 如果 write 追上了 read，说明满了，read 前移
        int64_t endPos = static_cast<int64_t>(startPos + needed);
        if (endPos > m_Capacity)
            endPos = static_cast<int64_t>(m_Capacity);

        // 检查 write 区域是否覆盖了 read 区域
        int64_t readEnd = m_ReadIndex + static_cast<int64_t>(sizeof(BlockInfo) +
            reinterpret_cast<BlockInfo*>(m_Buffer + m_ReadIndex)->Size);
        if (readEnd > static_cast<int64_t>(m_Capacity))
            readEnd = static_cast<int64_t>(m_Capacity);

        if (static_cast<int64_t>(startPos) == m_ReadIndex ||
            (static_cast<int64_t>(startPos) < readEnd &&
             static_cast<int64_t>(startPos) + static_cast<int64_t>(needed) > m_ReadIndex))
        {
            // 覆盖了最旧块，跳过它
            m_ReadIndex = (m_ReadIndex + sizeof(BlockInfo) +
                reinterpret_cast<BlockInfo*>(m_Buffer + m_ReadIndex)->Size) % m_Capacity;

            if (m_TotalBlocks > 0)
                m_TotalBlocks--;
        }
    }

    return startPos;
}

void RingBuffer::Push(const void* data, uint64_t len)
{
    if (!data || len == 0)
        return;

    std::lock_guard<std::mutex> lock(m_Mutex);

    uint64_t pos = AllocBlock(len);
    uint64_t writeLen = len;

    // 如果容量不够存储一个完整块，截断
    if (writeLen > m_Capacity - sizeof(BlockInfo))
        writeLen = m_Capacity - sizeof(BlockInfo);

    // 写入 BlockInfo
    BlockInfo info;
    info.Size = writeLen;
    std::memcpy(m_Buffer + pos, &info, sizeof(BlockInfo));

    // 写入数据（处理环绕）
    uint64_t dataPos = pos + sizeof(BlockInfo);
    if (dataPos + writeLen <= m_Capacity)
    {
        std::memcpy(m_Buffer + dataPos, data, writeLen);
    }
    else
    {
        uint64_t firstPart = m_Capacity - dataPos;
        std::memcpy(m_Buffer + dataPos, data, firstPart);
        std::memcpy(m_Buffer, static_cast<const uint8_t*>(data) + firstPart, writeLen - firstPart);
    }

    // 更新 WriteIndex
    m_WriteIndex = (pos + sizeof(BlockInfo) + writeLen) % m_Capacity;
    m_TotalBlocks++;
}

uint64_t RingBuffer::Available() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_TotalBlocks == 0)
        return m_Capacity;

    // 近似计算：write 和 read 之间剩余的空间
    // 简单估算：write 追到 read 之前的那段
    int64_t dist = static_cast<int64_t>(m_WriteIndex) - m_ReadIndex;
    if (dist < 0)
        dist += static_cast<int64_t>(m_Capacity);

    // 至少保留 sizeof(BlockInfo) 的空间作为安全边际
    uint64_t used = static_cast<uint64_t>(dist);
    if (used >= m_Capacity)
        return 0;
    return m_Capacity - used;
}

double RingBuffer::FillRatio() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_TotalBlocks == 0 || m_Capacity == 0)
        return 0.0;

    int64_t dist = static_cast<int64_t>(m_WriteIndex) - m_ReadIndex;
    if (dist < 0)
        dist += static_cast<int64_t>(m_Capacity);

    double ratio = static_cast<double>(dist) / static_cast<double>(m_Capacity);
    if (ratio > 1.0) ratio = 1.0;
    return ratio;
}

void RingBuffer::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::memset(m_Buffer, 0, m_Capacity);
    m_WriteIndex = 0;
    m_ReadIndex = -1;
    m_TotalBlocks = 0;
}

} // namespace X_Y
