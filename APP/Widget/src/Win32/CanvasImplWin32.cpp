#include "CanvasImpl.h"
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
namespace X_Y {

    class CanvasImplWin32 : public CanvasImpl {
    public:
        CanvasImplWin32(int w, int h, HDC hdc)
            : m_Width(w), m_Height(h), m_Handle(hdc),
              m_MemDC(nullptr), m_MemBitmap(nullptr), m_OldBitmap(nullptr)
        {
            // 双缓冲：创建与窗口 DC 兼容的内存 DC + 位图
            m_MemDC = ::CreateCompatibleDC(hdc);
            if (m_MemDC) {
                m_MemBitmap = ::CreateCompatibleBitmap(hdc, w, h);
                if (m_MemBitmap)
                    m_OldBitmap = (HBITMAP)::SelectObject(m_MemDC, m_MemBitmap);
            }
        }

        ~CanvasImplWin32() override {
            // 恢复旧对象再释放，避免 GDI 泄漏
            if (m_MemDC && m_OldBitmap)
                ::SelectObject(m_MemDC, m_OldBitmap);
            if (m_MemBitmap)
                ::DeleteObject(m_MemBitmap);
            if (m_MemDC)
                ::DeleteDC(m_MemDC);
        }

        int GetWidth() const override { return m_Width; }
        int GetHeight() const override { return m_Height; }

        // 双缓冲：把内存位图一次性 BitBlt 到窗口 DC
        void Flush() override {
            if (!m_MemDC || !m_Handle) return;
            ::BitBlt(m_Handle, 0, 0, m_Width, m_Height,
                     m_MemDC, 0, 0, SRCCOPY);
        }

        void FillRect(int x, int y, int w, int h, uint32_t
            color) override {
            RECT rect = { x, y, x + w, y + h };
            HBRUSH brush = CreateSolidBrush(RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            ::FillRect(m_MemDC, &rect, brush);
            DeleteObject(brush);
        }

        void DrawText(int x, int y, const char* text, uint32_t
            color) override {
            SetTextColor(m_MemDC, RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            SetBkMode(m_MemDC, TRANSPARENT);
            TextOutA(m_MemDC, x, y, text, (int)strlen(text));
        }

        void DrawText(int x, int y, const wchar_t* text,
            uint32_t color) override {
            SetTextColor(m_MemDC, RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            SetBkMode(m_MemDC, TRANSPARENT);
            TextOutW(m_MemDC, x, y, text, (int)wcslen(text));
        }

        void SetClip(int x, int y, int w, int h) override {
            HRGN rgn = CreateRectRgn(x, y, x + w, y + h);
            SelectClipRgn(m_MemDC, rgn);
            DeleteObject(rgn);
        }

        void ResetClip() override {
            SelectClipRgn(m_MemDC, nullptr);
        }

    private:
        int m_Width, m_Height;
        HDC m_Handle;
        HDC m_MemDC;
        HBITMAP m_MemBitmap;
        HBITMAP m_OldBitmap;
    };

    // 工厂实现
    CanvasImpl* CanvasFactory::CreateCanvasImpl(int w, int h,
        void* nativeHandle) {
        return new CanvasImplWin32(w, h, (HDC)nativeHandle);
    }

} // namespace X_Y