#include "../ProcessMemoryImpl.h"
#ifdef XY_PLATFORM_WINDOWS
#include "../Win32/ProcessMemoryWin32Impl.h"
#endif
namespace X_Y
{
    ProcessMemoryImpl *ProcessMemoryFactory::CreateProcessMemoryImpl()
    {
#ifdef XY_PLATFORM_WINDOWS
        return new ProcessMemoryWin32Impl();
#endif
    }
} // namespace X_Y
