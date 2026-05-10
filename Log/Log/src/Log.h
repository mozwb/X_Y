#pragma once
#include"configure.h"
#include"Logger.h"


namespace X_Y {
	struct LOG : public LogConfigure<LOG> {
		using LogConfigure<LOG>::LogConfigure;
	};
}
#ifdef XY_DEBUG
	#define LOG(logger, LEVEL, ...) \
		logger.Log<decltype(logger)::LEVEL>(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define log(logger, LEVEL, ...) \
		logger.log<decltype(logger)::LEVEL>(__VA_ARGS__)
#else
#define LOG(logger, LEVEL, ...)
#define log(logger, LEVEL, ...)
#endif