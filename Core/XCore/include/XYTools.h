#pragma once
#include<Log/include/XYLog.h>
#include<Buffer/include/Buffer.h>
#define XY_PROFILE_FUNCTION()\
        XTRACE("函数性能分析")\
		X_Y::StopWatch timer;