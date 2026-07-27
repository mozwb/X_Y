#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>
#include <string>
#include <sstream>
#include <cctype>
#include <functional>

namespace X_Y {

// ── Buffer ──
// 二进制数据容器：支持拷贝/移动/读写/自动扩容
// 提供自定义删除器（Deleter），支持 Pool 托管内存自动归还
//
// Deleter 是 std::function<void(uint8_t*, uint64_t, uint64_t)>
//   - 未设置（默认空）：走 bFreeInstead 判断
//   - 已设置：析构时调 Deleter(Data, Size, Capacity)，Data 置 nullptr
//   - 拷贝时 Deleter 不传递
//   - 移动时 Deleter 转移
//
// 重大约束：
//   Buffer 带 Deleter 时，调 Reserve/Ensure/Allocate 会先调 Release()
//   这会导致池内存被归还，之后再 new 新内存。这是预期行为：
//   "从池拿 → 追加不够了 → 自动切到自管内存"

struct Buffer
{
    uint8_t* Data = nullptr;
    uint64_t Size = 0;
    uint64_t Capacity = 0;

    // 释放方式：true = free(), false = delete[]
    bool bFreeInstead = false;

    // 自定义删除器（使用 std::function，支持捕获 this 的 lambda）
    using DeleterFn = std::function<void(uint8_t*, uint64_t, uint64_t)>;
    DeleterFn Deleter;

    Buffer() = default;
    explicit Buffer(uint64_t initialCapacity);

    Buffer(const Buffer& other);
    Buffer(Buffer&& other) noexcept;
    ~Buffer();

    Buffer& operator=(const Buffer& other);
    Buffer& operator=(Buffer&& other) noexcept;

    void Reserve(uint64_t newCapacity);
    void Ensure(uint64_t neededSize);
    void Allocate(uint64_t size);
    void Release();
    void ZeroInitialize();

    // ── 模板方法留在头文件 ──
    template<typename T>
    T* As(uint64_t offset = 0)
    {
        assert(offset + sizeof(T) <= (Size > 0 ? Size : Capacity));
        return reinterpret_cast<T*>(Data + offset);
    }

    template<typename T>
    const T* As(uint64_t offset = 0) const
    {
        assert(offset + sizeof(T) <= (Size > 0 ? Size : Capacity));
        return reinterpret_cast<const T*>(Data + offset);
    }

    template<typename T>
    T& Read(uint64_t offset = 0) { return *As<T>(offset); }

    template<typename T>
    const T& Read(uint64_t offset = 0) const { return *As<T>(offset); }

    template<typename T>
    void Write(uint64_t offset, const T& value)
    {
        uint64_t needed = offset + sizeof(T);
        if (needed > Size)
            Ensure(needed);
        memcpy(Data + offset, &value, sizeof(T));
        if (needed > Size)
            Size = needed;
    }

    void Append(const void* src, uint64_t len);

    template<typename T>
    void Append(const T& value) { Append(&value, sizeof(T)); }

    void Overwrite(const void* src, uint64_t len);
    void Overwrite(const Buffer& other);
    void Overwrite(std::initializer_list<uint8_t> list);
    void Overwrite(const char* str);

    Buffer View(uint64_t offset, uint64_t size) const;

    uint8_t& operator[](uint64_t index)
    {
        assert(index < Size);
        return Data[index];
    }

    const uint8_t& operator[](uint64_t index) const
    {
        assert(index < Size);
        return Data[index];
    }

    explicit operator bool() const { return Data != nullptr; }
    Buffer Copy() const { return Buffer(*this); }

    std::string toString() const;
};

// @@ BufferView：不拥有内存的只读视图
struct BufferView
{
    const uint8_t* Data = nullptr;
    uint64_t Size = 0;

    BufferView() = default;
    BufferView(const Buffer& buffer, uint64_t offset = 0, uint64_t size = 0)
        : Data(buffer.Data + offset)
        , Size(size == 0 ? (buffer.Size - offset) : size)
    {
    }

    template<typename T>
    const T& Read(uint64_t offset = 0) const
    {
        assert(offset + sizeof(T) <= Size);
        return *(const T*)(Data + offset);
    }

    const uint8_t& operator[](uint64_t index) const
    {
        assert(index < Size);
        return Data[index];
    }

    explicit operator bool() const { return Data != nullptr; }

    std::string toString() const;
};

} // namespace X_Y
