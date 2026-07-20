#include "PlatformLoop.h"
#include <windows.h>

namespace X_Y {

class PlatformLoopWin32 : public PlatformLoop {
public:
    bool PumpMessage() override {
        MSG msg{};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            return msg.message != WM_QUIT;
        }
        return true;
    }
};

PlatformLoop* PlatformLoopFactory::Create() {
    return new PlatformLoopWin32();
}

} 
// namespace X_Y
