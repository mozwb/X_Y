#include "Buffer.h"
#include <cstdio>

namespace X_Y {

// ── 构造/析构/拷贝/移动 ──

Buffer::Buffer(uint64_t initialCapacity)
{
    Reserve(initialCapacity);
}

Buffer::Buffer(const Buffer& other)
    : Data(nullptr), Size(0), Capacity(0)
    , bFreeInstead(false)
    // Deleter 默认空 — 拷贝不传递，新 Buffer 自管内存
{
    if (other.Data && other.Size > 0)
    {
        Reserve(other.Size);
        memcpy(Data, other.Data, other.Size);
        Size = other.Size;
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : Data(other.Data), Size(other.Size), Capacity(other.Capacity)
    , bFreeInstead(other.bFreeInstead)
    , Deleter(std::move(other.Deleter))  // 移动转移 Deleter
{
    other.Data = nullptr;
    other.Size = 0;
    other.Capacity = 0;
    other.bFreeInstead = false;
}

Buffer::~Buffer()
{
    Release();
}

Buffer& Buffer::operator=(const Buffer& other)
{
    if (this != &other)
    {
        Release();
        bFreeInstead = false;
        Deleter = nullptr;  // 拷贝赋值不传递
        if (other.Data && other.Size > 0)
        {
            Reserve(other.Size);
            memcpy(Data, other.Data, other.Size);
            Size = other.Size;
        }
    }
    return *this;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this != &other)
    {
        Release();
        Data = other.Data;
        Size = other.Size;
        Capacity = other.Capacity;
        bFreeInstead = other.bFreeInstead;
        Deleter = std::move(other.Deleter);
        other.Data = nullptr;
        other.Size = 0;
        other.Capacity = 0;
        other.bFreeInstead = false;
    }
    return *this;
}

// ── 内存管理 ──

void Buffer::Reserve(uint64_t newCapacity)
{
    if (newCapacity <= Capacity)
        return;

    newCapacity = (newCapacity + 15) & ~15ULL;
    uint8_t* newData = new uint8_t[newCapacity];
    if (Data)
    {
        memcpy(newData, Data, Size);
        Release();
    }
    Data = newData;
    Capacity = newCapacity;
    bFreeInstead = false;
    Deleter = nullptr;  // 重新分配后，新内存不归池管
}

void Buffer::Ensure(uint64_t neededSize)
{
    if (neededSize > Capacity)
    {
        uint64_t newCap = Capacity == 0 ? 64 : Capacity * 2;
        while (newCap < neededSize)
            newCap *= 2;
        Reserve(newCap);
    }
}

void Buffer::Allocate(uint64_t size)
{
    Release();
    Reserve(size);
    Size = size;
}

void Buffer::Release()
{
    if (!Data)
        return;

    if (Deleter)
    {
        // 池管理的内存 → 回调归还
        Deleter(Data, Size, Capacity);
    }
    else if (bFreeInstead)
    {
        free(Data);
    }
    else
    {
        delete[] Data;
    }

    Data = nullptr;
    Size = 0;
    Capacity = 0;
    bFreeInstead = false;
    Deleter = nullptr;
}

void Buffer::ZeroInitialize()
{
    if (Data)
        memset(Data, 0, Capacity);
}

// ── 读写操作 ──

void Buffer::Append(const void* src, uint64_t len)
{
    if (!src || len == 0) return;
    Ensure(Size + len);
    memcpy(Data + Size, src, len);
    Size += len;
}

void Buffer::Overwrite(const void* src, uint64_t len)
{
    if (!src && len > 0) return;
    Allocate(len);
    if (len > 0)
        memcpy(Data, src, len);
}

void Buffer::Overwrite(const Buffer& other)
{
    Overwrite(other.Data, other.Size);
}

void Buffer::Overwrite(std::initializer_list<uint8_t> list)
{
    Overwrite(list.begin(), list.size());
}

void Buffer::Overwrite(const char* str)
{
    uint64_t len = str ? strlen(str) : 0;
    Overwrite(reinterpret_cast<const uint8_t*>(str), len);
}

Buffer Buffer::View(uint64_t offset, uint64_t size) const
{
    assert(offset + size <= Size);
    Buffer result;
    result.Data = Data + offset;
    result.Size = size;
    return result;
}

// ── toString ──

std::string Buffer::toString() const
{
    if (!Data || Size == 0)
        return "";

    uint64_t printable = 0;
    uint64_t sampleLen = Size < 128 ? Size : 128;
    for (uint64_t i = 0; i < sampleLen; i++)
        if (isprint(Data[i]) || Data[i] == '\n' || Data[i] == '\t')
            printable++;

    std::ostringstream oss;
    if (printable * 2 > Size)
    {
        uint64_t showLen = Size < 256 ? Size : 256;
        for (uint64_t i = 0; i < showLen; i++)
        {
            if (Data[i] == '\n') oss << "\\n";
            else if (Data[i] == '\t') oss << "\\t";
            else if (isprint(Data[i])) oss << (char)Data[i];
            else oss << '.';
        }
        if (Size > 256) oss << "...";
    }
    else
    {
        uint64_t showLen = Size < 32 ? Size : 32;
        for (uint64_t i = 0; i < showLen; i++)
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", Data[i]);
            oss << buf;
        }
        if (Size > 32) oss << "...";
    }
    return oss.str();
}

// ── BufferView::toString ──

std::string BufferView::toString() const
{
    if (!Data || Size == 0)
        return "";

    std::ostringstream oss;
    uint64_t showLen = Size < 24 ? Size : 24;
    for (uint64_t i = 0; i < showLen; i++)
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", Data[i]);
        oss << buf;
    }
    if (Size > 24) oss << "...";
    return oss.str();
}

} // namespace X_Y
