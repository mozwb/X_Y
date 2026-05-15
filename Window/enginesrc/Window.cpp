#include "Window.h"

#ifdef XY_PLATFORM_WINDOWS
//#include "Platform/Windows/WindowsWindow.h"
#endif


namespace X_Y {
	Scope<Window> Window::Create(const WindowProps& props)
	{
	#ifdef XY_PLATFORM_WINDOWS
		//return CreateScope<WindowsWindow>(props);
	#else
		XY_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}