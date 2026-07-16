#include "Container/Canvas.h"
#include <cstring>

namespace X_Y {

    Canvas::Canvas(int w, int h, CanvasHandle handle)
        : m_Handle(handle), m_Width(w), m_Height(h)
    {}

    int Canvas::GetWidth() const { return m_Width; }
    int Canvas::GetHeight() const { return m_Height; }

    void Canvas::FillRect(int x, int y, int w, int h, uint32_t color) {
        RECT rect = { x, y, x + w, y + h };
        HBRUSH brush = CreateSolidBrush(RGB(
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF
        ));
        ::FillRect(m_Handle, &rect, brush);
        DeleteObject(brush);
    }

    void Canvas::DrawText(int x, int y, const char* text, uint32_t color) {
        SetTextColor(m_Handle, RGB(
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF
        ));
        SetBkMode(m_Handle, TRANSPARENT);
        TextOutA(m_Handle, x, y, text, (int)strlen(text));
    }

    void Canvas::DrawText(int x, int y, const wchar_t* text, uint32_t color) {
        SetTextColor(m_Handle, RGB(
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF
        ));
        SetBkMode(m_Handle, TRANSPARENT);
        TextOutW(m_Handle, x, y, text, (int)wcslen(text));
    }

    void Canvas::SetClip(int x, int y, int w, int h) {
        HRGN rgn = CreateRectRgn(x, y, x + w, y + h);
        SelectClipRgn(m_Handle, rgn);
        DeleteObject(rgn);
    }

    void Canvas::ResetClip() {
        SelectClipRgn(m_Handle, nullptr);
    }

}
