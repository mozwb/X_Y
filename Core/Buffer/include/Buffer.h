#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>
#include <sstream>
#include <cctype>

namespace X_Y {

	// @@ 砚台注：用 Buffer 做日志的缓冲池 + 二进制数据容器
	// @@ 改进后的 Buffer：支持拷贝、移动、读写、子视图
	struct Buffer
	{
		uint8_t* Data = nullptr;
		uint64_t Size = 0;
		// ✅ 添加容量字段，支持自动扩容
		uint64_t Capacity = 0;

		Buffer() = default;

		// ✅ 不再要求初始 size，默认 64 字节起步
		explicit Buffer(uint64_t initialCapacity)
		{
			Reserve(initialCapacity);
		}

		Buffer(const Buffer& other)
			: Data(nullptr), Size(0), Capacity(0)
		{
			if (other.Data && other.Size > 0)
			{
				Reserve(other.Size);
				memcpy(Data, other.Data, Size);
				Size = other.Size;
			}
		}

		Buffer(Buffer&& other) noexcept
			: Data(other.Data), Size(other.Size), Capacity(other.Capacity)
		{
			other.Data = nullptr;
			other.Size = 0;
			other.Capacity = 0;
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
					Reserve(other.Size);
					memcpy(Data, other.Data, other.Size);
					Size = other.Size;
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
				Capacity = other.Capacity;
				other.Data = nullptr;
				other.Size = 0;
				other.Capacity = 0;
			}
			return *this;
		}

		// ✅ 按需扩容（2 倍增长策略）
		void Reserve(uint64_t newCapacity)
		{
			if (newCapacity <= Capacity)
				return;

			// 向上对齐到 16 字节，减少重分配
			newCapacity = (newCapacity + 15) & ~15ULL;
			uint8_t* newData = new uint8_t[newCapacity];
			if (Data)
			{
				memcpy(newData, Data, Size);
				delete[] Data;
			}
			Data = newData;
			Capacity = newCapacity;
		}

		// ✅ 确保能写入 count 字节
		void Ensure(uint64_t neededSize)
		{
			if (neededSize > Capacity)
			{
				uint64_t newCap = Capacity == 0 ? 64 : Capacity * 2;
				while (newCap < neededSize)
					newCap *= 2;
				Reserve(newCap);
			}
		}

		void Allocate(uint64_t size)
		{
			Release();
			Reserve(size);
			Size = size;
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
			Capacity = 0;
		}

		void ZeroInitialize()
		{
			if (Data)
				memset(Data, 0, Capacity);
		}

		// ✅ 写入时自动扩容
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
			uint64_t needed = offset + sizeof(T);
			if (needed > Size)
				Ensure(needed);
			memcpy(Data + offset, &value, sizeof(T));
			if (needed > Size)
				Size = needed;
		}

		// ✅ 追加数据到末尾
		void Append(const void* src, uint64_t len)
		{
			if (!src || len == 0) return;
			Ensure(Size + len);
			memcpy(Data + Size, src, len);
			Size += len;
		}

		template<typename T>
		void Append(const T& value)
		{
			Append(&value, sizeof(T));
		}

		Buffer View(uint64_t offset, uint64_t size) const
		{
			assert(offset + size <= Size);
			Buffer result;
			result.Data = Data + offset;
			result.Size = size;
			return result;
		}

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

		Buffer Copy() const
		{
			return Buffer(*this);
		}

		// ✅ 直接打印内容，不加 "Buffer[size]:" 前缀
		std::string toString() const
		{
			if (!Data || Size == 0)
				return "";

			// 判断是否像文本
			uint64_t printable = 0;
			uint64_t sampleLen = Size < 128 ? Size : 128;
			for (uint64_t i = 0; i < sampleLen; i++)
				if (isprint(Data[i]) || Data[i] == '\n' || Data[i] == '\t')
					printable++;

			std::ostringstream oss;
			if (printable * 2 > Size)
			{
				// 文本模式
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
				// 十六进制模式
				uint64_t showLen = Size < 32 ? Size : 32;
				for (uint64_t i = 0; i < showLen; i++)
				{
					// ✅ snprintf 跨平台兼容
					char buf[4];
					snprintf(buf, sizeof(buf), "%02X ", Data[i]);
					oss << buf;
				}
				if (Size > 32) oss << "...";
			}
			return oss.str();
		}
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

		// @@ 日志输出
		std::string toString() const
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
	};

}
