#pragma once
#include<Log/include/XYLog.h>
#include<Buffer/include/Buffer.h>
#include<Timer/include/Timer.h>

namespace X_Y {

	// @@ ProfileScope：作用域性能分析 RAII 类，析构时自动打印函数名 + 耗时
	// @@ 基础用法：XY_PROFILE_FUNCTION()
	// @@ 追加额外信息：XY_PROFILE_FUNCTION() << "count=100" << "batch=32"
	struct ProfileScope {
		StopWatch timer;
		std::string funcName;
		std::string extraInfo;

		ProfileScope(const char* func) : funcName(func) {}

		~ProfileScope() {
			if (extraInfo.empty())
				XPINK("[{}] 耗时 {:.2f}ms", funcName, timer.Milliseconds())
			else  
				XPINK("[{}] 耗时 {:.2f}ms | {}", funcName, timer.Milliseconds(), extraInfo)
		}

		// @@ 用 << 链式追加额外信息，输出格式："[func] 耗时 xx.xxms | info1 | info2 | ..."
		ProfileScope& operator<<(const std::string& info) {
			if (!extraInfo.empty()) extraInfo += " | ";
			extraInfo += info;
			return *this;
		}
	};

}

// @@ 宏辅助：拼接 token
#define XY_PASTE2(a, b) a##b
#define XY_PASTE(a, b) XY_PASTE2(a, b)

// @@ 创建 ProfileScope 变量，变量名 = prof_ + __LINE__，防止嵌套冲突
// @@ 示例：
//   void Foo() { XY_PROFILE_FUNCTION(); ... }  // 自动打印 [Foo] 耗时 3.45ms
//   void Bar(int n) {
//       XY_PROFILE_FUNCTION() << "n=" + std::to_string(n);
//       ...  // 自动打印 [Bar] 耗时 5.12ms | n=42
//   }
#define XY_PROFILE_FUNCTION() \
		X_Y::ProfileScope XY_PASTE(prof_, __LINE__)(__FUNCTION__);
