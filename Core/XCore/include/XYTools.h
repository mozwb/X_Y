#pragma once
#include<Log/include/XYLog.h>
#include<Buffer/include/Buffer.h>

// ✅ 记录函数名 + 耗时
#define XY_PROFILE_FUNCTION() \
		XTRACE("→ {} 进入", __FUNCTION__) \
		X_Y::StopWatch XY_CONCAT(timer_, __LINE__); \
		struct XY_CONCAT(profile_end_, __LINE__) { \
			~XY_CONCAT(profile_end_, __LINE__)() { \
				XTRACE("← {} 结束, 耗时{}ms", __FUNCTION__, XY_CONCAT(timer_, __LINE__).Milliseconds()); \
			} \
		} XY_CONCAT(profile_end_, __LINE__);

// ✅ 辅助宏：拼接标识符（避免宏展开问题）
#define XY_CONCAT(a, b) XY_CONCAT_INNER(a, b)
#define XY_CONCAT_INNER(a, b) a##b

// ✅ Buffer 内容拼接工具 — 把多个数据写入 Buffer
// $$ 使用示例：XY_BUFFER(buf) << "hello" << 42 << 3.14f;
struct BufferWriter
{
	X_Y::Buffer& buf;
	BufferWriter(X_Y::Buffer& b) : buf(b) {}

	BufferWriter& operator<<(const char* str)
	{
		buf.Append(str, strlen(str));
		return *this;
	}

	BufferWriter& operator<<(const std::string& str)
	{
		buf.Append(str.data(), str.size());
		return *this;
	}

	template<typename T>
	BufferWriter& operator<<(const T& val)
	{
		std::ostringstream oss;
		oss << val;
		std::string s = oss.str();
		buf.Append(s.data(), s.size());
		return *this;
	}
};

#define XY_BUFFER(buf) BufferWriter(buf)
