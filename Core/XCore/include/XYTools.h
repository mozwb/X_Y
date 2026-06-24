#pragma once
#include<Log/include/XYLog.h>
#include<Buffer/include/Buffer.h>
#include<Timer/include/Timer.h>

namespace X_Y {

	// 作用域性能分析 RAII 类：析构时自动打印函数名 + 耗时 + 自定义额外信息
	struct ProfileScope {
		StopWatch timer;
		std::string funcName;
		std::string extraInfo;

		ProfileScope(const char* func) : funcName(func) {}

		~ProfileScope() {
			if (extraInfo.empty())
				XTRACE("[{}] 耗时 {:.2f}ms", funcName, timer.Milliseconds());
			else
				XTRACE("[{}] 耗时 {:.2f}ms | {}", funcName, timer.Milliseconds(), extraInfo);
		}

		// 用 << 追加额外信息
		ProfileScope& operator<<(const std::string& info) {
			if (!extraInfo.empty()) extraInfo += " | ";
			extraInfo += info;
			return *this;
		}
	};

}

// 宏辅助：拼接 token
#define XY_PASTE2(a, b) a##b
#define XY_PASTE(a, b) XY_PASTE2(a, b)

// 创建 ProfileScope 变量，自动注入当前函数名
#define XY_PROFILE_FUNCTION() \
		X_Y::ProfileScope XY_PASTE(prof_, __LINE__)(__FUNCTION__)
