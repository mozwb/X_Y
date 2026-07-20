#include "CanvasImpl.h"
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
namespace X_Y {

    class CanvasImplWin32 : public CanvasImpl {
    public:
        CanvasImplWin32(int w, int h, HDC hdc)
            : m_Width(w), m_Height(h), m_Handle(hdc)
        {
        }

        int GetWidth() const override { return m_Width; }
        int GetHeight() const override { return m_Height; }

        void FillRect(int x, int y, int w, int h, uint32_t
            color) override {
            RECT rect = { x, y, x + w, y + h };
            HBRUSH brush = CreateSolidBrush(RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            ::FillRect(m_Handle, &rect, brush);
            DeleteObject(brush);
        }

        void DrawText(int x, int y, const char* text, uint32_t
            color) override {
            SetTextColor(m_Handle, RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            SetBkMode(m_Handle, TRANSPARENT);
            TextOutA(m_Handle, x, y, text, (int)strlen(text));
        }

        void DrawText(int x, int y, const wchar_t* text,
            uint32_t color) override {
            SetTextColor(m_Handle, RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            SetBkMode(m_Handle, TRANSPARENT);
            TextOutW(m_Handle, x, y, text, (int)wcslen(text));
        }

        void SetClip(int x, int y, int w, int h) override {
            HRGN rgn = CreateRectRgn(x, y, x + w, y + h);
            SelectClipRgn(m_Handle, rgn);
            DeleteObject(rgn);
        }

        void ResetClip() override {
            SelectClipRgn(m_Handle, nullptr);
        }

    private:
        int m_Width, m_Height;
        HDC m_Handle;
    };

    // 工厂实现
    CanvasImpl* CanvasFactory::CreateCanvasImpl(int w, int h,
        void* nativeHandle) {
        return new CanvasImplWin32(w, h, (HDC)nativeHandle);
    }

} // namespace X_Y