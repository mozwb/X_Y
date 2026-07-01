#include<Window/include/BaseWIn.h>
#include"Log/include/XYLog.h"
#include<Window/include/WinCore.h>
namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
	bool BaseWin::Show(showtype nshow) {
		if (m_Nhwd) {
			ShowWindow(m_Nhwd, nshow);
			UpdateWindow(m_Nhwd);
			return true;
		}
		return false;
	}

	void BaseWin::Close() {
		XINFO("调用colse函数")
		if (m_Nhwd)	this->Show(HIDE);
	}
	void BaseWin::Destroy() {
		if (m_Nhwd) {
			DestroyWindow(m_Nhwd);
			m_Nhwd = nullptr;
		}
	}
	void BaseWin::SetTitle(const char* title) {
		if(m_Nhwd)  SetWindowTextA(m_Nhwd, title);
	}
    bool BaseWin::Create(const char* title, uint width, uint height) {
        if (width == 0) width = 800;
        if (height == 0) height = 600;
        HINSTANCE hInstance = X_Y::WinCore::g_hInstance;

        // 1. 把 char* 变量安全转为宽字符串
        wchar_t wTitle[256] = { 0 };

        // 🔥 这里必须用 CP_ACP（Windows 本地编码）
        MultiByteToWideChar(CP_ACP, 0, title, -1, wTitle, _countof(wTitle));

        HWND hwnd = CreateWindowEx(
            0,
            X_Y::WinCore::g_szClassName,
            wTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            width, height,
            NULL, NULL,
            hInstance,
            this
        );
        if (!hwnd) {
            DWORD err = GetLastError();
            XERROR("CreateWindowEx failed, error code: {}", err);
            return false;
        }
        XINFO("hwnd:{}", (uint64_t)hwnd)
            if (hwnd) {
                this->SetNHWD(hwnd);
                return true;
            }
        return false;
	}
#endif
}