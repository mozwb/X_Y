#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>

namespace X_Y {

	// $$ 改进后的 Buffer：支持拷贝、移动、读写、子视图
	struct Buffer
	{
		uint8_t* Data = nullptr;
		uint64_t Size = 0;

		Buffer() = default;

		explicit Buffer(uint64_t size)
			: Data(nullptr), Size(0)
		{
			Allocate(size);
		}

		Buffer(const Buffer& other)
			: Data(nullptr), Size(0)
		{
			if (other.Data && other.Size > 0)
			{
				Allocate(other.Size);
				memcpy(Data, other.Data, Size);
			}
		}

		Buffer(Buffer&& other) noexcept
			: Data(other.Data), Size(other.Size)
		{
			other.Data = nullptr;
			other.Size = 0;
		}

		~Buffer()
		{
			Release();
		}

		Buffer& operator=(const Buffer& other)
		{
			if (this != &other)
			{
				Release();
				if (other.Data && other.Size > 0)
				{
					Allocate(other.Size);
					memcpy(Data, other.Data, Size);
				}
			}
			return *this;
		}

		Buffer& operator=(Buffer&& other) noexcept
		{
			if (this != &other)
			{
				Release();
				Data = other.Data;
				Size = other.Size;
				other.Data = nullptr;
				other.Size = 0;
			}
			return *this;
		}

		void Allocate(uint64_t size)
		{
			Release();
			if (size > 0)
			{
				Data = new uint8_t[size];
				Size = size;
			}
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		void ZeroInitialize()
		{
			if (Data)
				memset(Data, 0, Size);
		}

		// 读写辅助
		template<typename T>
		T& Read(uint64_t offset = 0)
		{
			assert(offset + sizeof(T) <= Size);
			return *(T*)(Data + offset);
		}

		template<typename T>
		const T& Read(uint64_t offset = 0) const
		{
			assert(offset + sizeof(T) <= Size);
			return *(T*)(Data + offset);
		}

		template<typename T>
		void Write(uint64_t offset, const T& value)
		{
			assert(offset + sizeof(T) <= Size);
			memcpy(Data + offset, &value, sizeof(T));
		}

		// 子视图（不拥有内存）
		Buffer View(uint64_t offset, uint64_t size) const
		{
			assert(offset + size <= Size);
			Buffer result;
			result.Data = Data + offset;
			result.Size = size;
			return result;
		}

		// 运算符重载
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

		// 工具方法
		Buffer Copy() const
		{
			return Buffer(*this);
		}
	};

	// $$ 不拥有内存的视图，适合传递子缓冲区
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
	};

}
