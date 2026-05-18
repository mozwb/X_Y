#pragma once
#include"Log/src/LogConfigure.h"

#ifdef XY_PLATFORM_WINDOWS

#include<windows.h>
namespace X_Y {
    using LogConfigure::DEVICE;

    struct Console : public DEVICE {
        explicit Console(std::string title);
        ~Console() override;

        void Log(const std::string& msg) const override;
        std::string toString() const override;
    };
}
#endif