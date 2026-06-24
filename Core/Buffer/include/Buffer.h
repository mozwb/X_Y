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

		// @@ Log 模块 To_Str() 使用，自动识别文本/二进制输出
		std::string toString() const
		{
			if (!Data || Size == 0)
				return "Buffer: null";

			std::ostringstream oss;
			oss << "Buffer[" << Size << "]: ";

			// 判断是否像文本
			uint64_t printable = 0;
			for (uint64_t i = 0; i < Size && i < 64; i++)
				if (isprint(Data[i]) || Data[i] == '\n' || Data[i] == '\t')
					printable++;

			if (printable * 2 > Size)
			{
				// 文本模式
				oss << "\"";
				for (uint64_t i = 0; i < Size && i < 128; i++)
				{
					if (Data[i] == '\n') oss << "\\n";
					else if (Data[i] == '\t') oss << "\\t";
					else if (isprint(Data[i])) oss << (char)Data[i];
					else oss << '.';
				}
				oss << "\"";
				if (Size > 128) oss << "...(" << Size << ")";
			}
			else
			{
				// 十六进制模式
				for (uint64_t i = 0; i < Size && i < 32; i++)
				{
					char buf[4];
					sprintf_s(buf, "%02X ", Data[i]);
					oss << buf;
				}
				if (Size > 32) oss << "...(" << Size << " bytes)";
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
				return "BufferView: null";

			std::ostringstream oss;
			oss << "BufferView[" << Size << "]: ";
			for (uint64_t i = 0; i < Size && i < 24; i++)
			{
				char buf[4];
				sprintf_s(buf, "%02X ", Data[i]);
				oss << buf;
			}
			if (Size > 24) oss << "...";
			return oss.str();
		}
	};

}
