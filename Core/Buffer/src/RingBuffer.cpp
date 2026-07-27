#include "RingBuffer.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace X_Y {

RingBuffer::RingBuffer(uint64_t capacity)
{
    // 利用 Buffer::Reserve 分配内存，统一内存管理
    Reserve(capacity);
    m_WriteIndex = 0;
    m_ReadIndex = -1;
    m_TotalBlocks = 0;
    std::memset(Data, 0, Capacity);
}

RingBuffer::RingBuffer(RingBuffer&& other) noexcept
    : Buffer(std::move(other))
    , m_WriteIndex(other.m_WriteIndex)
    , m_ReadIndex(other.m_ReadIndex)
    , m_TotalBlocks(other.m_TotalBlocks)
    // m_Mutex 不移动（新对象有自己的 mutex）
{
    other.m_WriteIndex = 0;
    other.m_ReadIndex = -1;
    other.m_TotalBlocks = 0;
}

RingBuffer& RingBuffer::operator=(RingBuffer&& other) noexcept
{
    if (this != &other)
    {
        Buffer::operator=(std::move(other));
        m_WriteIndex = other.m_WriteIndex;
        m_ReadIndex = other.m_ReadIndex;
        m_TotalBlocks = other.m_TotalBlocks;
        other.m_WriteIndex = 0;
        other.m_ReadIndex = -1;
        other.m_TotalBlocks = 0;
    }
    return *this;
}

RingBuffer::~RingBuffer()
{
    // Buffer 析构自动释放
}

uint64_t RingBuffer::AllocBlock(uint64_t dataLen)
{
    uint64_t needed = sizeof(BlockInfo) + dataLen;

    if (needed > Capacity)
        dataLen = Capacity - sizeof(BlockInfo);
    needed = sizeof(BlockInfo) + dataLen;

    uint64_t startPos = m_WriteIndex;

    // 如果从 startPos 到结尾不够放 → 折返
    if (startPos + needed > Capacity)
    {
        std::memset(Data + startPos, 0, Capacity - startPos);
        startPos = 0;
    }

    // 首次写入设 ReadIndex
    if (m_ReadIndex == -1)
        m_ReadIndex = static_cast<int64_t>(startPos);

    // 检查是否会覆盖最旧块
    if (m_TotalBlocks > 0)
    {
        int64_t readEnd = m_ReadIndex + static_cast<int64_t>(sizeof(BlockInfo) +
            reinterpret_cast<BlockInfo*>(Data + m_ReadIndex)->Size);
        if (readEnd > static_cast<int64_t>(Capacity))
            readEnd = static_cast<int64_t>(Capacity);

        if (static_cast<int64_t>(startPos) == m_ReadIndex ||
            (static_cast<int64_t>(startPos) < readEnd &&
             static_cast<int64_t>(startPos) + static_cast<int64_t>(needed) > m_ReadIndex))
        {
            m_ReadIndex = (m_ReadIndex + sizeof(BlockInfo) +
                reinterpret_cast<BlockInfo*>(Data + m_ReadIndex)->Size) % Capacity;

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

    if (writeLen > Capacity - sizeof(BlockInfo))
        writeLen = Capacity - sizeof(BlockInfo);

    BlockInfo info;
    info.Size = writeLen;
    std::memcpy(Data + pos, &info, sizeof(BlockInfo));

    uint64_t dataPos = pos + sizeof(BlockInfo);
    if (dataPos + writeLen <= Capacity)
    {
        std::memcpy(Data + dataPos, data, writeLen);
    }
    else
    {
        uint64_t firstPart = Capacity - dataPos;
        std::memcpy(Data + dataPos, data, firstPart);
        std::memcpy(Data, static_cast<const uint8_t*>(data) + firstPart, writeLen - firstPart);
    }

    m_WriteIndex = (pos + sizeof(BlockInfo) + writeLen) % Capacity;
    m_TotalBlocks++;
}

void RingBuffer::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::memset(Data, 0, Capacity);
    m_WriteIndex = 0;
    m_ReadIndex = -1;
    m_TotalBlocks = 0;
}

uint64_t RingBuffer::Available() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_TotalBlocks == 0)
        return Capacity;

    int64_t dist = static_cast<int64_t>(m_WriteIndex) - m_ReadIndex;
    if (dist < 0)
        dist += static_cast<int64_t>(Capacity);

    uint64_t used = static_cast<uint64_t>(dist);
    if (used >= Capacity)
        return 0;
    return Capacity - used;
}

double RingBuffer::FillRatio() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_TotalBlocks == 0 || Capacity == 0)
        return 0.0;

    int64_t dist = static_cast<int64_t>(m_WriteIndex) - m_ReadIndex;
    if (dist < 0)
        dist += static_cast<int64_t>(Capacity);

    double ratio = static_cast<double>(dist) / static_cast<double>(Capacity);
    if (ratio > 1.0) ratio = 1.0;
    return ratio;
}

} // namespace X_Y
