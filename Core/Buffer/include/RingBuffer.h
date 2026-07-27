#pragma once
#include "Buffer.h"
#include <mutex>

namespace X_Y {

// ── RingBuffer ──
// 循环 Buffer：固定容量，写入超出时自动覆盖最旧数据
// 继承 Buffer 复用内存管理（统一 malloc/free 或 Pool 分配）
// 线程安全（internal mutex）
//
// 典型用途：日志缓冲区、流式数据（实时显示最近 N 条记录）
//
// 使用方式：
//   RingBuffer ring(65536);              // 64KB 环形缓冲区
//   ring.Push("hello", 5);               // 追加数据
//   ring.Read([](const uint8_t* d, uint64_t sz) { ... }); // 遍历所有有效数据

class RingBuffer : public Buffer {
public:
    explicit RingBuffer(uint64_t capacity = 65536);
    RingBuffer(RingBuffer&& other) noexcept;
    RingBuffer& operator=(RingBuffer&& other) noexcept;
    ~RingBuffer();

    // ── 写入 ──
    // 追加一条数据到环形缓冲区
    // 如果剩余空间不足 len，自动覆盖最旧数据腾出空间
    void Push(const void* data, uint64_t len);

    // ── 读取 ──
    // 遍历所有有效数据，按原始写入的块依次回调
    // callback(data, size) 对每个数据块调用
    // 返回处理的数据块数
    template<typename Fn>
    uint64_t Read(Fn callback) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (m_TotalBlocks == 0)
            return 0;

        uint64_t count = 0;
        int64_t idx = m_ReadIndex;
        for (uint64_t i = 0; i < m_TotalBlocks; i++)
        {
            BlockInfo* info = reinterpret_cast<BlockInfo*>(Data + idx);
            if (info->Size == 0 || info->Size > Capacity)
                break;

            uint64_t dataStart = idx + sizeof(BlockInfo);
            uint64_t dataEnd = dataStart + info->Size;

            if (dataEnd <= Capacity)
            {
                callback(Data + dataStart, info->Size);
            }
            else
            {
                uint64_t firstPart = Capacity - dataStart;
                callback(Data + dataStart, firstPart);
                callback(Data, info->Size - firstPart);
            }

            idx = (idx + info->TotalBlockSize()) % Capacity;
            count++;
        }
        return count;
    }

    // ── 重置 ──
    void Clear();
    void Reset() { Clear(); }

    // ── 查询 ──
    uint64_t TotalBlocks() const { return m_TotalBlocks; }
    bool Empty() const { return m_TotalBlocks == 0; }
    uint64_t Available() const;
    double FillRatio() const;

private:
#pragma pack(push, 1)
    struct BlockInfo {
        uint64_t Size;
        uint64_t TotalBlockSize() const {
            return sizeof(BlockInfo) + Size;
        }
    };
#pragma pack(pop)

    uint64_t AllocBlock(uint64_t dataLen);

    // 写入指针（下一个 BlockInfo 写入位置）
    uint64_t m_WriteIndex = 0;
    // 读取指针：指向第一个有效块
    int64_t  m_ReadIndex = -1;
    // 有效数据块数
    uint64_t m_TotalBlocks = 0;

    mutable std::mutex m_Mutex;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
};

} // namespace X_Y
