#include "CanvasImpl.h"
#include "Widget/include/Font.h"
#include "Dpi.h"
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
namespace X_Y {

    class CanvasImplWin32 : public CanvasImpl {
    public:
        CanvasImplWin32(int w, int h, HDC hdc)
            : m_Width(w), m_Height(h), m_Handle(hdc),
              m_MemDC(nullptr), m_MemBitmap(nullptr), m_OldBitmap(nullptr),
              m_CurrentFont(nullptr)
        {
            m_Scale = Dpi::GetScale();
            // 位图用物理像素大小；逻辑尺寸 = 物理 / scale，供上层按逻辑布局。
            m_LogicW = (int)(w / m_Scale);
            m_LogicH = (int)(h / m_Scale);
            if (m_LogicW < 1) m_LogicW = 1;
            if (m_LogicH < 1) m_LogicH = 1;

            // 双缓冲：创建与窗口 DC 兼容的内存 DC + 位图（物理像素）
            m_MemDC = ::CreateCompatibleDC(hdc);
            if (m_MemDC) {
                m_MemBitmap = ::CreateCompatibleBitmap(hdc, w, h);
                if (m_MemBitmap)
                    m_OldBitmap = (HBITMAP)::SelectObject(m_MemDC, m_MemBitmap);
            }
            // 默认字体：微软雅黑（ClearType 抗锯齿在上层 SetFont 时选定）
            ApplyDefaultFont();
        }

        ~CanvasImplWin32() override {
            // 恢复旧对象再释放，避免 GDI 泄漏
            if (m_MemDC && m_CurrentFont)
                ::SelectObject(m_MemDC, ::GetStockObject(DEFAULT_GUI_FONT));
            if (m_MemDC && m_OldBitmap)
                ::SelectObject(m_MemDC, m_OldBitmap);
            if (m_CurrentFont)
                ::DeleteObject(m_CurrentFont);
            if (m_MemBitmap)
                ::DeleteObject(m_MemBitmap);
            if (m_MemDC)
                ::DeleteDC(m_MemDC);
        }

        int GetWidth() const override { return m_LogicW; }
        int GetHeight() const override { return m_LogicH; }

        // 双缓冲：把内存位图一次性 BitBlt 到窗口 DC
        void Flush() override {
            if (!m_MemDC || !m_Handle) return;
            ::BitBlt(m_Handle, 0, 0, m_Width, m_Height,
                     m_MemDC, 0, 0, SRCCOPY);
        }

        // 设置绘制字体（Font 包装内部持有 HFONT）
        void SetFont(const Font& font) override {
            if (!m_MemDC) return;
            if (m_CurrentFont)
                ::SelectObject(m_MemDC, ::GetStockObject(DEFAULT_GUI_FONT));
            HFONT hf = (HFONT)font.GetNativeHandle();
            if (hf)
                ::SelectObject(m_MemDC, hf);
            m_CurrentFont = hf;
        }

        void FillRect(int x, int y, int w, int h, uint32_t
            color) override {
            int px = S(x), py = S(y), pw = S(w), ph = S(h);
            RECT rect = { px, py, px + pw, py + ph };
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
            // UTF-8 → UTF-16，统一走宽字符（配合全局 /utf-8）
            std::wstring ws = Utf8ToWide(text);
            TextOutW(m_MemDC, S(x), S(y), ws.c_str(), (int)ws.size());
        }

        void DrawText(int x, int y, const wchar_t* text,
            uint32_t color) override {
            SetTextColor(m_MemDC, RGB(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF
            ));
            SetBkMode(m_MemDC, TRANSPARENT);
            TextOutW(m_MemDC, S(x), S(y), text, (int)wcslen(text));
        }

        // 组合拳：铺背景 + 写字（窄版，UTF-8 转宽再走宽版实现）
        void FillText(int x, int y, int w, int h, int tx, int ty,
            const char* text, uint32_t textColor, uint32_t
            bgColor) override {
            std::wstring ws = Utf8ToWide(text);
            if (ws.empty()) return;
            FillText(x, y, w, h, tx, ty, ws.c_str(), textColor,
                bgColor);
        }

        // 组合拳：铺背景 + 写字（宽版核心）
        void FillText(int x, int y, int w, int h, int tx, int ty,
            const wchar_t* text, uint32_t textColor, uint32_t
            bgColor) override {
            // 1. 铺背景（底是啥不用管，反正我们填）
            FillRect(x, y, w, h, bgColor);
            // 2. 文字透明背景 = 同一背景色（ClearType 亚像素需已知背景）
            SetBkMode(m_MemDC, OPAQUE);
            SetBkColor(m_MemDC, RGB(
                (bgColor >> 16) & 0xFF,
                (bgColor >> 8) & 0xFF,
                bgColor & 0xFF
            ));
            // 3. 写文字（起点用 tx,ty，可与背景错开）
            SetTextColor(m_MemDC, RGB(
                (textColor >> 16) & 0xFF,
                (textColor >> 8) & 0xFF,
                textColor & 0xFF
            ));
            TextOutW(m_MemDC, S(tx), S(ty), text, (int)wcslen(text));
            // 4. 恢复 TRANSPARENT，防止污染后续绘制
            SetBkMode(m_MemDC, TRANSPARENT);
        }

        void SetClip(int x, int y, int w, int h) override {
            int px = S(x), py = S(y), pw = S(w), ph = S(h);
            HRGN rgn = CreateRectRgn(px, py, px + pw, py + ph);
            SelectClipRgn(m_MemDC, rgn);
            DeleteObject(rgn);
        }

        void ResetClip() override {
            SelectClipRgn(m_MemDC, nullptr);
        }

    private:
        // 逻辑坐标 → 物理像素（DPI 缩放）
        int S(int v) const {
            return (int)(v * m_Scale + 0.5f);
        }

        // 用微软雅黑作为默认绘制字体（ClearType 抗锯齿）
        void ApplyDefaultFont() {
            if (!m_MemDC) return;

            LOGFONTW lf = { 0 };
            std::wstring family = L"Microsoft YaHei UI";
            if (family.size() >= (size_t)LF_FACESIZE)
                family.resize(LF_FACESIZE - 1);
            wcscpy_s(lf.lfFaceName, family.c_str());
            lf.lfHeight = -(int)(14 * m_Scale + 0.5f);  // 14px 逻辑字号，物理放大
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;   // 抗锯齿
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfOutPrecision = OUT_TT_PRECIS;

            HFONT f = ::CreateFontIndirectW(&lf);
            if (f) {
                ::SelectObject(m_MemDC, f);
                m_CurrentFont = f;
            }
        }

        // 简单 UTF-8 → UTF-16 转换（配合全局 /utf-8 编译，全链路 UTF-8）
        static std::wstring Utf8ToWide(const char* text) {
            if (!text) return L"";
            int len = ::MultiByteToWideChar(CP_UTF8, 0, text, -1,
                nullptr, 0);
            if (len <= 0) return L"";
            std::wstring ws(len, L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, text, -1, &ws[0], len);
            if (!ws.empty() && ws.back() == L'\0')
                ws.pop_back();
            return ws;
        }

        int m_Width, m_Height;      // 物理像素（位图/窗口尺寸）
        int m_LogicW, m_LogicH;     // 逻辑尺寸（供上层布局）
        float m_Scale = 1.0f;       // DPI 缩放系数
        HDC m_Handle;
        HDC m_MemDC;
        HBITMAP m_MemBitmap;
        HBITMAP m_OldBitmap;
        HFONT m_CurrentFont = nullptr;
    };

    // 工厂实现
    CanvasImpl* CanvasFactory::CreateCanvasImpl(int w, int h,
        void* nativeHandle) {
        return new CanvasImplWin32(w, h, (HDC)nativeHandle);
    }

} // namespace X_Y