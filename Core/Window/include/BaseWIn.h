#pragma once
#include<string>
#ifdef XY_PLATFORM_WINDOWS
#include<windows.h>
#define NativeWindow  HWND
#elif 
    ERROR("不支持其他os")
#endif

#define NHWD NativeWindow
namespace X_Y {
        typedef unsigned int uint;
        enum showtype {
            HIDE,
            NORMAL,
            MINIMIZED,
            MAXIMIZED,
            NOACTIVATE,
            SHOW,
            MINIMIZE,
            MINNOACTIVE,
            SHOWNA,
            RESTORE,
            DEFAULT
        };
    class BaseWin
    {
    public:
        BaseWin() = default;
        void SetNHWD(NHWD& hwnd) { m_Nhwd = hwnd; }
        NHWD GetNHWD()const { return m_Nhwd; }
        NHWD GetNativeWindow()const { return GetNHWD();}
    protected:
        bool Show(showtype nshow=SHOW);
        void Close();
        void SetTitle(const char* title);
        void Destroy();
        virtual bool Create(const char* title, uint width, uint height);
        virtual std::string toString() const { return "BaseWindow"; }
    private:
        NHWD m_Nhwd = nullptr;
    };
}