#pragma once
namespace X_Y {
#include<string>
#ifdef XY_PLATFORM_WINDOWS
#include<windows.h>
    //只负责管理HWND和消息分发，不负责任何实现
    class BaseWin
    {
    public:
        BaseWin() = default;
        void SetHwnd(HWND& hwnd) { m_hwnd = hwnd; }
        HWND GetHwnd()const { return m_hwnd; }
        HWND GetNativeWindow()const { return GetHwnd();}
        virtual std::string toString() const { return "BaseWindow"; }
    private:
        HWND m_hwnd = nullptr;
    };
#endif
}