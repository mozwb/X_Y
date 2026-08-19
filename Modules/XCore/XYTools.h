#pragma once
#include <Log/XYLog.h>
#include "Memory/Buffer.h"
#include "Timer/Timer.h"
#include <optional>

#ifdef XY_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace X_Y
{

	// @@ ProfileScope：作用域性能分析 RAII 类，析构时自动打印函数名 + 耗时
	// @@ 基础用法：XY_PROFILE_FUNCTION()
	// @@ 追加额外信息：XY_PROFILE_FUNCTION() << "count=100" << "batch=32"
	struct ProfileScope
	{
		StopWatch timer;
		std::string funcName;
		std::string extraInfo;

		ProfileScope(const char *func) : funcName(func) {}

		~ProfileScope()
		{
			if (extraInfo.empty())
				XPINK("[{}] 耗时 {:.2f}ms", funcName, timer.Milliseconds())
			else
				XPINK("[{}] 耗时 {:.2f}ms | {}", funcName, timer.Milliseconds(), extraInfo)
		}

		// @@ 用 << 链式追加额外信息，输出格式："[func] 耗时 xx.xxms | info1 | info2 | ..."
		ProfileScope &operator<<(const std::string &info)
		{
			if (!extraInfo.empty())
				extraInfo += " | ";
			extraInfo += info;
			return *this;
		}
	};

}

// @@ 宏辅助：拼接 token
#define XY_PASTE2(a, b) a##b
#define XY_PASTE(a, b) XY_PASTE2(a, b)

#ifdef ENABLEPROFILE
// @@ 创建 ProfileScope 变量，变量名 = prof_ + __LINE__，防止嵌套冲突
// @@ 示例：
//   void Foo() { XY_PROFILE_FUNCTION(); ... }  // 自动打印 [Foo] 耗时 3.45ms
//   void Bar(int n) {
//       XY_PROFILE_FUNCTION() << "n=" + std::to_string(n);
//       ...  // 自动打印 [Bar] 耗时 5.12ms | n=42
//   }
#define XY_PROFILE_FUNCTION() \
	X_Y::ProfileScope XY_PASTE(prof_, __LINE__)(__FUNCTION__);
#else
#define XY_PROFILE_FUNCTION()
#endif

#ifdef XY_PLATFORM_WINDOWS
// GBK(CP_ACP) → std::wstring(UTF‑16)
inline std::optional<std::wstring> gbk_to_wstring(const char *gbk)
{
	if (!gbk || *gbk == '\0')
		return std::wstring{};

	int wlen = MultiByteToWideChar(CP_ACP, 0, gbk, -1, nullptr, 0);
	if (wlen <= 0)
	{
		return std::nullopt;
	}
	std::wstring wstr(wlen, 0);
	MultiByteToWideChar(CP_ACP, 0, gbk, -1, wstr.data(), wlen);
	return wstr;
}

// UTF‑16 wstring → UTF‑8 std::string
inline std::optional<std::string> wstring_to_utf8(const std::wstring &wstr)
{
	if (wstr.empty())
		return std::string{};

	int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (u8len <= 0)
	{
		return std::nullopt;
	}
	std::string utf8(u8len, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8.data(), u8len, nullptr, nullptr);
	return utf8;
}

// 对外接口：GBK char* → UTF‑8 std::string
inline std::optional<std::string> gbk_to_utf8(const char *gbk_str)
{
	auto wopt = gbk_to_wstring(gbk_str);
	if (!wopt)
		return std::nullopt;
	return wstring_to_utf8(*wopt);
}

// 重载 std::string
inline std::optional<std::string> gbk_to_utf8(const std::string &gbk_str)
{
	return gbk_to_utf8(gbk_str.c_str());
}

#endif
