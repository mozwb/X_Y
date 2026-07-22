#include "BaseWin.h"
#ifdef XY_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace X_Y {

BaseWin::BaseWin()
    : m_Impl(PlatformFactory::CreateWindowImpl())
{
}

BaseWin::~BaseWin() {
    Destroy();
}

// ── 窗口生命周期 ──────────────────────────────

bool BaseWin::Create(const char* title, uint width, uint height,
                     WindowStyleFlag style, void* parentHandle)
{
    if (!m_Impl) return false;
    return m_Impl->Create(title, width, height, style, parentHandle, this);
}

bool BaseWin::Show(ShowCmd nshow) {
    return m_Impl ? m_Impl->Show(nshow) : false;
}

void BaseWin::Close() {
    if (m_Impl) m_Impl->Close();
}

void BaseWin::Destroy() {
    if (m_Impl) {
        m_Impl->Destroy();
        m_NativeHandle = nullptr;
    }
}

void BaseWin::SetTitle(const char* title) {
    if (m_Impl) m_Impl->SetTitle(title);
}

// ── 窗口信息 ──────────────────────────────────

void BaseWin::GetScreenRect(int& left, int& top, int& right, int& bottom) const {
    if (m_Impl) m_Impl->GetClientRect(left, top, right, bottom);
}

void* BaseWin::GetParentNativeHandle() const {
    if (m_Impl) return m_Impl->GetParentNativeHandle();
    return nullptr;
}

bool BaseWin::SetParent(void* newParent) {
    return m_Impl ? m_Impl->SetParent(newParent) : false;
}

// ── 坐标转换 ──────────────────────────────────

void BaseWin::ScreenToClient(int& x, int& y) const {
    if (m_Impl) m_Impl->ScreenToClient(x, y);
}

void BaseWin::ClientToScreen(int& x, int& y) const {
    if (m_Impl) m_Impl->ClientToScreen(x, y);
}

// ── 鼠标 & 光标 ───────────────────────────────

void BaseWin::CaptureMouse() {
    if (m_Impl) m_Impl->CaptureMouse();
}

void BaseWin::ReleaseMouseCapture() {
    if (m_Impl) m_Impl->ReleaseMouseCapture();
}

void BaseWin::SetCursorStyle(CursorStyle style) {
    if (m_Impl) m_Impl->SetCursorStyle(style);
}

void BaseWin::MoveAndResize(int x, int y, int w, int h, bool noZOrder) {
    if (m_Impl) m_Impl->MoveAndResize(x, y, w, h, noZOrder);
}

void BaseWin::RequestRepaint() {
    if (m_Impl) m_Impl->RequestRepaint();
}

void BaseWin::GetMouseScreenPos(int& x, int& y) {
    WindowImpl::GetMouseScreenPos(x, y);
}

BaseWin* BaseWin::GetWindowAt(int screenX, int screenY) {
#ifdef XY_PLATFORM_WINDOWS
    POINT pt = { screenX, screenY };
    HWND hwnd = ::WindowFromPoint(pt);
    if (!hwnd) return nullptr;
    return (BaseWin*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
#else
    return nullptr;
#endif
}

} // namespace X_Y
