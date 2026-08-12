#include "WindowImpl.h"
#include "Win32/WindowImplWin32.h"

namespace X_Y {

WindowImpl* PlatformFactory::CreateWindowImpl() {
#ifdef XY_PLATFORM_WINDOWS
    return CreateWindowImplWin32();
#elif defined(XY_PLATFORM_LINUX)
    // TODO
    return nullptr;
#elif defined(XY_PLATFORM_MACOS)
    // TODO
    return nullptr;
#else
#error "Unsupported platform"
#endif
}

} // namespace X_Y
