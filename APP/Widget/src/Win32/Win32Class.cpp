#include "Win32/Win32Class.h"
#include "Win32/Win32Globals.h"
#include "Win32/Win32WndProc.h"


namespace X_Y::Win32 {

    namespace {
        bool g_ClassRegistered = false;
    }

    void RegisterWinClass(HINSTANCE hInstance) {
        if (g_ClassRegistered) return;
        g_hInstance = hInstance;

        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.hInstance = hInstance;
        wc.lpszClassName = g_szClassName;
        wc.lpfnWndProc = StaticWndProc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszMenuName = nullptr;

        if (!RegisterClassEx(&wc)) {
            MessageBox(nullptr, _T("窗口类注册失败"), _T("错误"), MB_ICONERROR);
            exit(EXIT_FAILURE);
        }

        g_ClassRegistered = true;
    }

} // namespace X_Y::Win32