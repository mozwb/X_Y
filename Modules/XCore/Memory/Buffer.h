#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>
#include <string>
#include <sstream>
#include <cctype>
#include <span>
#include <type_traits>
namespace X_Y
{

    struct BufferView;
    // ── Buffer ──
    // 二进制数据容器：支持拷贝/移动/读写/自动扩容
    // 所有内存由 Memory 统一管理（Reserve / Release 走 Memory::Alloc / Memory::Free）
    struct Buffer
    {
        uint8_t *Data = nullptr;
        uint64_t Size = 0;
        uint64_t Capacity = 0;

        Buffer() = default;
        explicit Buffer(uint64_t initialCapacity);

        Buffer(const Buffer &other);
        Buffer(Buffer &&other) noexcept;
        ~Buffer();

        Buffer &operator=(const Buffer &other);
        Buffer &operator=(Buffer &&other) noexcept;

        bool Reserve(uint64_t newCapacity);
        bool Ensure(uint64_t neededSize);
        void Allocate(uint64_t size);
        void Release();
        void ZeroInitialize();

        // ── 模板方法留在头文件 ──
        template <typename T>
        T *As(uint64_t offset = 0)
        {
            assert(offset + sizeof(T) <= (Size > 0 ? Size : Capacity));
            return reinterpret_cast<T *>(Data + offset);
        }

        template <typename T>
        const T *As(uint64_t offset = 0) const
        {
            assert(offset + sizeof(T) <= (Size > 0 ? Size : Capacity));
            return reinterpret_cast<const T *>(Data + offset);
        }

        template <typename T>
        T &Read(uint64_t offset = 0) { return *As<T>(offset); }

        template <typename T>
        const T &Read(uint64_t offset = 0) const { return *As<T>(offset); }

        template <typename T>
        void Write(uint64_t offset, const T &value)
        {
            if (offset > UINT64_MAX - sizeof(T))
                return;
            uint64_t needed = offset + sizeof(T);
            if (needed > Size)
            {
                if (!Ensure(needed))
                    return;
            }
            memcpy(Data + offset, &value, sizeof(T));
            if (needed > Size)
                Size = needed;
        }
        void WriteBytes(uint64_t offset, const void *src, uint64_t byteCount)
        {
            if (byteCount == 0)
                return;
            // 防溢出：offset + byteCount 算术溢出
            if (offset > UINT64_MAX - byteCount)
                return;

            uint64_t needed = offset + byteCount;
            if (needed > Size)
            {
                if (!Ensure(needed))
                    return;
            }
            memcpy(Data + offset, src, byteCount);
            if (needed > Size)
                Size = needed;
        }
        void Append(const void *src, uint64_t len);

        template <typename T>
        void Append(const T &value) { Append(&value, sizeof(T)); }

        void Overwrite(const void *src, uint64_t len);
        void Overwrite(const Buffer &other);
        void Overwrite(std::initializer_list<uint8_t> list);
        void Overwrite(const char *str);

        // 只读视图：不拥有内存的观察者，析构不释放。写请用 BufferWriter。

        BufferView View(uint64_t offset, uint64_t size) const;

        uint8_t &operator[](uint64_t index)
        {
            assert(index < Size);
            return Data[index];
        }

        const uint8_t &operator[](uint64_t index) const
        {
            assert(index < Size);
            return Data[index];
        }

        explicit operator bool() const { return Data != nullptr; }
        Buffer Copy() const { return Buffer(*this); }

        std::string toString() const;

        // 添加视图方法
        /// @param offset 字节偏移
        /// @param elementCount 元素个数；传0自动取到buffer末尾（只取完整T，丢弃尾部不足一个T的零碎字节）
        template <typename T>
        std::span<T> Span(uint64_t offset, uint64_t elementCount = 0)
        {
            static_assert(std::is_trivially_copyable_v<T>, "Only trivially‑copyable POD");
            assert(offset <= Size);

            if (elementCount == 0)
            {
                // 剩余字节
                const uint64_t byteRemain = Size - offset;
                // 最多能容纳多少完整 T
                elementCount = byteRemain / sizeof(T);
            }

            const uint64_t totalBytesNeeded = offset + elementCount * sizeof(T);
            assert(totalBytesNeeded <= Size);

            T *ptr = As<T>(offset);
            return std::span<T>{ptr, elementCount};
        }

        template <typename T>
        std::span<const T> Span(uint64_t offset, uint64_t elementCount = 0) const
        {
            static_assert(std::is_trivially_copyable_v<T>, "Only trivially‑copyable POD");
            assert(offset <= Size);

            if (elementCount == 0)
            {
                const uint64_t byteRemain = Size - offset;
                elementCount = byteRemain / sizeof(T);
            }

            const uint64_t totalBytesNeeded = offset + elementCount * sizeof(T);
            assert(totalBytesNeeded <= Size);

            const T *ptr = As<T>(offset);
            return std::span<const T>{ptr, elementCount};
        }
    };

    // @@ BufferView：不拥有内存的只读视图
    struct BufferView
    {
        const uint8_t *Data = nullptr;
        uint64_t Size = 0;

        BufferView() = default;
        BufferView(const Buffer &buffer, uint64_t offset = 0, uint64_t size = 0)
            : Data(buffer.Data + offset), Size(size == 0 ? (buffer.Size - offset) : size)
        {
            // 保护：offset 不能越过 Size，size 不能越界
            assert(offset <= buffer.Size);
            if (size == 0)
                assert(offset <= buffer.Size);
            else
                assert(offset + size <= buffer.Size);
            (void)buffer;
        }

        template <typename T>
        const T &Read(uint64_t offset = 0) const
        {
            assert(offset + sizeof(T) <= Size);
            return *(const T *)(Data + offset);
        }

        const uint8_t &operator[](uint64_t index) const
        {
            assert(index < Size);
            return Data[index];
        }

        explicit operator bool() const { return Data != nullptr; }

        std::string toString() const;
    };

}
// namespace X_Y
