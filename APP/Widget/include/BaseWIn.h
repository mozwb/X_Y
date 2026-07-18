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

        uint GetActualWidth() const { return m_ActualWidth; }
        uint GetActualHeight() const { return m_ActualHeight; }
        void SetActualSize(uint width, uint height) { m_ActualWidth = width; m_ActualHeight = height; }

        // ── 跨平台工具方法 ──────────────────────────────
        void GetScreenRect(int& left, int& top, int& right, int& bottom) const;
        void ScreenToClient(int& x, int& y) const;
        void ClientToScreen(int& x, int& y) const;
        void CaptureMouse();
        static void ReleaseMouseCapture();
        NHWD GetParentNHWD() const;

        enum class CursorStyle { Arrow, SizeWE, SizeNS, SizeAll, Hand, IBeam };
        void SetCursorStyle(CursorStyle style);
        // 设置窗口位置和大小（可指定保持 z-order）
        void MoveAndResize(int x, int y, int w, int h, bool noZOrder = true);

        static void GetMouseScreenPos(int& x, int& y);
        static BaseWin* GetWindowAt(int screenX, int screenY);

    protected:
        bool Show(showtype nshow=SHOW);
        void Close();
        void SetTitle(const char* title);
        void Destroy();
        virtual bool Create(const char* title, uint width, uint height);
        virtual std::string toString() const { return "BaseWindow"; }
    private:
        NHWD m_Nhwd = nullptr;
        uint m_ActualWidth = 0;
        uint m_ActualHeight = 0;
    };
}
