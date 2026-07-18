#include<Widget/include/BaseWIn.h>
#include"Log/include/XYLog.h"
#include<Widget/include/WinCore.h>
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

    // ════════════════════════════════════════════════════════
    // 跨平台工具方法
    // ════════════════════════════════════════════════════════

#ifdef XY_PLATFORM_WINDOWS
    void BaseWin::GetScreenRect(int& left, int& top, int& right, int& bottom) const
    {
        if (!m_Nhwd) { left = top = right = bottom = 0; return; }
        RECT rc;
        ::GetWindowRect(m_Nhwd, &rc);
        left   = rc.left;
        top    = rc.top;
        right  = rc.right;
        bottom = rc.bottom;
    }

    void BaseWin::ScreenToClient(int& x, int& y) const
    {
        if (!m_Nhwd) return;
        POINT pt = { x, y };
        ::ScreenToClient(m_Nhwd, &pt);
        x = pt.x;
        y = pt.y;
    }

    void BaseWin::ClientToScreen(int& x, int& y) const
    {
        if (!m_Nhwd) return;
        POINT pt = { x, y };
        ::ClientToScreen(m_Nhwd, &pt);
        x = pt.x;
        y = pt.y;
    }

    void BaseWin::CaptureMouse()
    {
        if (m_Nhwd)
            ::SetCapture(m_Nhwd);
    }

    void BaseWin::ReleaseMouseCapture()
    {
        ::ReleaseCapture();
    }

    NHWD BaseWin::GetParentNHWD() const
    {
        return m_Nhwd ? ::GetParent(m_Nhwd) : nullptr;
    }

    void BaseWin::SetCursorStyle(CursorStyle style)
    {
        LPCWSTR id = IDC_ARROW;
        switch (style) {
            case CursorStyle::Arrow:  id = IDC_ARROW; break;
            case CursorStyle::SizeWE: id = IDC_SIZEWE; break;
            case CursorStyle::SizeNS: id = IDC_SIZENS; break;
            case CursorStyle::SizeAll: id = IDC_SIZEALL; break;
            case CursorStyle::Hand:   id = IDC_HAND; break;
            case CursorStyle::IBeam:  id = IDC_IBEAM; break;
        }
        HCURSOR hCursor = ::LoadCursor(nullptr, id);
        if (hCursor)
            ::SetCursor(hCursor);
    }

    void BaseWin::MoveAndResize(int x, int y, int w, int h, bool noZOrder)
    {
        if (!m_Nhwd) return;
        UINT flags = SWP_SHOWWINDOW;
        if (noZOrder) flags |= SWP_NOZORDER;
        ::SetWindowPos(m_Nhwd, nullptr, x, y, w, h, flags);
    }

    void BaseWin::GetMouseScreenPos(int& x, int& y)
    {
        POINT pt;
        ::GetCursorPos(&pt);
        x = pt.x;
        y = pt.y;
    }

    BaseWin* BaseWin::GetWindowAt(int screenX, int screenY)
    {
        POINT pt = { screenX, screenY };
        HWND hwnd = ::WindowFromPoint(pt);
        if (!hwnd) return nullptr;
        return (BaseWin*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
#endif // XY_PLATFORM_WINDOWS
}