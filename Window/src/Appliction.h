#pragma once
#include"Log/src/XYLog.h"
#ifdef XY_PLATFORM_WINDOWS
#include"Window/src/windows/WinCore.h"
namespace X_Y{
    class Application
    {
    private:
        static Application* s_instance;
    public:
        Application(int argc, char* argv[])
        {
            s_instance = this;
        }

        ~Application()
        {
            s_instance = nullptr;
        }

        // 消息循环 → app.exec()
        int exec()
        {
            MSG msg{};
            while (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            return (int)msg.wParam;
        }

        static Application* instance()
        {
            return s_instance;
        }
    };
    inline Application* Application::s_instance = nullptr;
}
#else
    ERROR("仅支持windows系统")
#endif