#pragma once
#include"Log/src/LogConfigure.h"
#ifdef XY_DEBUG
namespace X_Y {
	template<typename T> using LogBase = LogConfigure::LogConfigure<T>;
	using	LogConfigure::DEVICE;
	using LogConfigure::TIME;
	using LogConfigure::DATE;
	struct LOG : public LogBase<LOG> {
		using LogBase<LOG>::LogBase;
	};
}
	static X_Y::LOG logger("log");
	#define LOG(logger, LEVEL,...) \
			logger.Log<decltype(logger)::LEVEL>(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
	#define INFO(...)\
			LOG(logger, Info, __VA_ARGS__)
	#define TRACE(...)\
			LOG(logger, Trace, __VA_ARGS__)
	#define WARN(...)\
			LOG(logger, Warn, __VA_ARGS__)
	#define DEBUG(...)\
			LOG(logger, Debug, __VA_ARGS__)
	#define ERROR(...)\
			LOG(logger, Error, __VA_ARGS__)
	#define FATAL(...)\
			LOG(logger, Fatal, __VA_ARGS__)

	#if defined(XY_PLATFORM_WINDOWS)
	#define XY_DEBUGBREAK() __debugbreak()
	#elif defined(XY_PLATFORM_LINUX)
	#define XY_DEBUGBREAK() __builtin_trap()
	#elif defined(XY_PLATFORM_MAC)
	#define XY_DEBUGBREAK() __builtin_trap()
	#else
	#define XY_DEBUGBREAK()
	#endif

	#define XY_CORE_ASSERT(condition, message) \
			if (!(condition)) { \
				FATAL("[ASSERT FAILED]:{}",message); \
				XY_DEBUGBREAK(); \
			}



#else
	#define LOG(logger, LEVEL, ...)
	#define INFO(...)
	#define TRACE(...)
	#define TRACE(...)\
	#define DEBUG(...)
	#define ERROR(...)
	#define FATAL(...)

	#define XY_CORE_ASSERT(condition, message)
	#define XY_DEBUGBREAK()
#endif
//使用示例
//
//struct MyLogger : public X_Y::LogBase<MyLogger> {
//	using X_Y::LogBase<MyLogger>::LogBase;
//};
//
//struct XLogger : public X_Y::LogBase<XLogger> {
//	using X_Y::LogBase<XLogger>::LogBase;
//	LOG_LEVEL_EXT(Oh)
//		LOG_LEVEL_API(Oh)
//		template<typename ...Args>
//	XLogger(Args ...args) : X_Y::LogBase<XLogger>(args ...) {
//		Oh::setModel("%255:0:255%[位置][日期][时间][发起者][等级]:[内容]%#");
// 	str format(str mod, str lev, str content, str file, str line, str func) {
//		//重写这个可以自定义模板匹配方式
//}
//str PiPei(str s, vec<str> args) {
//	//重写这个可以自定义参数匹配模式
//}
// 
//	}
//};
//struct NewDEvice : public X_Y::DEVICE {
//	std::string toString() const override {
//		return "NewDevice";
//	}
//	void Log(const std::string& message)const override {
//		std::cout << "NewDevice Log: " << message << std::endl;
//	}
//};
//int main() {
//	X_Y::LOG a("LOG");
//
//	MyLogger logger("MyLogger");
//	XLogger xlogger("XLogger");
//	std::cout << MyLogger::Trace::getModel() << std::endl;
//	std::cout << XLogger::Oh::getModel() << std::endl;
//	std::cout << XLogger::Oh::toString() << std::endl;
//	std::cout << xlogger << std::endl;
//	xlogger.log<XLogger::Oh>("Hello, %!", "World");
//
//	logger.add(new NewDEvice());
//
//	INFO("aaaa")
//		TRACE("aaaa")
//		DEBUG("aaaa")
//		WARN("aaaa")
//
//		ERROR("aaaa")
//		FATAL("aaaa")
//
//
//		return 0;
//}