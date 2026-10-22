#pragma once
#include "Log/Log.h"

namespace X_Y
{
    class CoreLog : public LogConfigure::LogConfigure<CoreLog>
    {
        LOG_ALL_API

        EngineLogger()
            : LogConfigure<EngineLogger>("Engine")
        {
            // 添加控制台输出设备
            add(std::make_unique<LogConfigure::DEVICE>());
        }

        static EngineLogger &Instance()
        {
            static EngineLogger inst;
            return inst;
        }
    };
}