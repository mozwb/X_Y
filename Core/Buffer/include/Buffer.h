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
// 所有自管内存统一使用 malloc/free（不再有 new[]/delete[]）
//
// 删除器（Deleter）：
//   - 未设置（默认空）：析构时走 free(Data)
//   - 已设置：析构时调 Deleter(Data, Size, Capacity)（用于 Pool 内存自动归还）
//   - 拷贝时不传递 Deleter
//   - 移动时转移 Deleter
//
// Buffer 带 Deleter 时调 Reserve/Ensure/Allocate：
//   先 Release（归还池内存）→ 再 malloc 新内存 → 清除 Deleter
//   这是安全行为："池内存不够用就切到自管"

struct Buffer
{
    uint8_t* Data = nullptr;
    uint64_t Size = 0;
    uint64_t Capacity = 0;

    // 自定义删除器（std::function，支持捕获 this 的 lambda）
    // 有 → 析构调删除器（池管理内存）
    // 无 → 析构走 free()（所有自管内存统一 malloc）
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
