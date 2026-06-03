#include"Log/include/XYLog.h"

#ifdef XY_PLATFORM_WINDOWS
	#include"WinCore.h"
	#include"winConfigure.h"
	extern int main(int argc, char* argv[]);
		int WINAPI WinMain(
			_In_ HINSTANCE hInstance,//实例句柄
			_In_opt_ HINSTANCE hPrevInstance,//没用
			_In_ LPSTR     lpCmdLine,//命令行参数字符串
			_In_ int       nCmdShow//显示窗口的方式
	) {

		X_Y::allowConsole();
		XINFO("程序运行到入口")
		X_Y::WinCore::RegisterWinClass(hInstance);
		X_Y::WinCore::SetHinstace(hInstance);
		return main(__argc, __argv);
		}
#else
		ERROR("仅支持windows系统")
#endif