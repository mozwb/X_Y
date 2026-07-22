#include "Win32/WindowImplWin32.h"
#include "Win32/Win32Globals.h"
#include <windows.h>
#include <cstdint>

namespace X_Y {

// ── 窗口风格转换 ──────────────────────────────────────
static DWORD TranslateWindowStyle(WindowStyleFlag style) {
    DWORD ws = 0;
    if (HasFlag(style, WindowStyleFlag::Overlapped))   ws |= WS_OVERLAPPEDWINDOW;
    if (HasFlag(style, WindowStyleFlag::Child))         ws |= WS_CHILD;
    if (HasFlag(style, WindowStyleFlag::Popup))         ws |= WS_POPUP;
    if (HasFlag(style, WindowStyleFlag::Borderless))    ws |= WS_POPUP;
    if (HasFlag(style, WindowStyleFlag::Resizable))     ws |= WS_SIZEBOX;
    if (HasFlag(style, WindowStyleFlag::ClipChildren))  ws |= WS_CLIPCHILDREN;
    if (HasFlag(style, WindowStyleFlag::ClipSiblings))  ws |= WS_CLIPSIBLINGS;
    if (HasFlag(style, WindowStyleFlag::Visible))       ws |= WS_VISIBLE;
    // 默认窗口：如果没有指定任何风格，给 Overlapped
    if (ws == 0) ws = WS_OVERLAPPEDWINDOW;
    return ws;
}

class WindowImplWin32 : public WindowImpl {
public:
    WindowImplWin32() : m_Hwnd(nullptr) {}
    ~WindowImplWin32() override { Destroy(); }

    bool Create(const char* title, uint32_t width, uint32_t height,
                WindowStyleFlag style, void* parentHandle,
                void* createParam) override
    {
        if (width == 0) width = 800;
        if (height == 0) height = 600;

        wchar_t wTitle[256] = { 0 };
        MultiByteToWideChar(CP_ACP, 0, title, -1, wTitle, _countof(wTitle));

        DWORD dwStyle = TranslateWindowStyle(style);

        int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
        if (dwStyle & WS_CHILD) { x = 0; y = 0; }

        HWND hwnd = CreateWindowEx(
            0, Win32::g_szClassName, wTitle,
            dwStyle, x, y, width, height,
            (HWND)parentHandle, nullptr,
            Win32::g_hInstance,
            createParam
        );

        if (!hwnd) return false;

        m_Hwnd = hwnd;
        return true;
    }

    bool Show(ShowCmd cmd) override {
        if (!m_Hwnd) return false;
        static const int nShowMap[] = {
            SW_HIDE, SW_NORMAL, SW_MINIMIZE, SW_MAXIMIZE, SW_SHOW, SW_SHOWDEFAULT
        };
        int idx = static_cast<int>(cmd);
        ShowWindow(m_Hwnd, (idx >= 0 && idx < 6) ? nShowMap[idx] : SW_SHOW);
        UpdateWindow(m_Hwnd);
        return true;
    }

    void Close() override {
        if (m_Hwnd) Show(ShowCmd::Hide);
    }

    void Destroy() override {
        if (m_Hwnd) {
            DestroyWindow(m_Hwnd);
            m_Hwnd = nullptr;
        }
    }

    void SetTitle(const char* title) override {
        if (m_Hwnd) SetWindowTextA(m_Hwnd, title);
    }

    void* GetNativeHandle() const override { return m_Hwnd; }
    void* GetParentNativeHandle() const override {
        return m_Hwnd ? ::GetParent(m_Hwnd) : nullptr;
    }

    bool SetParent(void* newParent) override {
        if (!m_Hwnd) return false;
        ::SetParent(m_Hwnd, (HWND)newParent);
        return true;
    }

    void GetClientRect(int& l, int& t, int& r, int& b) const override {
        if (!m_Hwnd) { l = t = r = b = 0; return; }
        RECT rc; ::GetClientRect(m_Hwnd, &rc);
        l = rc.left; t = rc.top; r = rc.right; b = rc.bottom;
    }

    uint32_t GetClientWidth() const override {
        if (!m_Hwnd) return 0;
        RECT rc; ::GetClientRect(m_Hwnd, &rc);
        return rc.right - rc.left;
    }

    uint32_t GetClientHeight() const override {
        if (!m_Hwnd) return 0;
        RECT rc; ::GetClientRect(m_Hwnd, &rc);
        return rc.bottom - rc.top;
    }

    void SetClientSize(uint32_t width, uint32_t height) override {}

    void ScreenToClient(int& x, int& y) const override {
        if (!m_Hwnd) return;
        POINT pt = { x, y }; ::ScreenToClient(m_Hwnd, &pt);
        x = pt.x; y = pt.y;
    }

    void ClientToScreen(int& x, int& y) const override {
        if (!m_Hwnd) return;
        POINT pt = { x, y }; ::ClientToScreen(m_Hwnd, &pt);
        x = pt.x; y = pt.y;
    }

    void CaptureMouse() override { if (m_Hwnd) ::SetCapture(m_Hwnd); }
    void ReleaseMouseCapture() override { ::ReleaseCapture(); }

    void SetCursorStyle(CursorStyle style) override {
        static const LPCWSTR ids[] = {
            IDC_ARROW, IDC_SIZEWE, IDC_SIZENS,
            IDC_SIZEALL, IDC_HAND, IDC_IBEAM
        };
        int idx = static_cast<int>(style);
        HCURSOR hCursor = ::LoadCursor(nullptr,
            (idx >= 0 && idx < 6) ? ids[idx] : IDC_ARROW);
        if (hCursor) ::SetCursor(hCursor);
    }

    void MoveAndResize(int x, int y, int w, int h, bool noZOrder) override {
        if (!m_Hwnd) return;
        UINT flags = SWP_SHOWWINDOW | (noZOrder ? SWP_NOZORDER : 0);
        ::SetWindowPos(m_Hwnd, nullptr, x, y, w, h, flags);
    }

    void RequestRepaint() override {
        if (m_Hwnd) ::InvalidateRect(m_Hwnd, nullptr, TRUE);
    }

private:
    HWND m_Hwnd = nullptr;
};

// ── 静态工具实现 ──────────────────────────────
void WindowImpl::GetMouseScreenPos(int& x, int& y) {
    POINT pt; ::GetCursorPos(&pt);
    x = pt.x; y = pt.y;
}

void WindowImpl::ReleaseGlobalMouseCapture() {
    ::ReleaseCapture();
}

// ── 平台创建函数 ──────────────────────────────
WindowImpl* CreateWindowImplWin32() {
    return new WindowImplWin32();
}

} // namespace X_Y
