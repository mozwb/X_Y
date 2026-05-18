#include"Log/src/LogDeviceExt.h"
#ifdef XY_PLATFORM_WINDOWS
namespace X_Y {

    Console::Console(std::string title) {
        // 1. 创建控制台
        AllocConsole();

        // 2. 强制重定向标准句柄
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetStdHandle(STD_OUTPUT_HANDLE, hConsole);
        SetStdHandle(STD_ERROR_HANDLE, hConsole);

        // 3. 绑定 C 标准输出
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        // 4. 绑定 C++ 标准流
        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::wcout.clear();
        std::cin.clear();
        std::wcin.clear();

        // 5. 设置窗口标题
        SetConsoleTitleA(title.c_str());
    }

    Console::~Console() {
        FreeConsole();
    }

    void Console::Log(const std::string& msg) const {
        // 🔥 直接输出！超级简单！
        std::cout << msg << std::endl;
    }

    std::string Console::toString() const {
        return "控制台llll";
    }

}
#endif
