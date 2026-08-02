#include "Widget/include/Font.h"
#include <windows.h>

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // 把枚举质量映射到 Win32 LOGFONT.lfQuality
    static BYTE ToGdiQuality(FontDesc::Quality q) {
        switch (q) {
            case FontDesc::NonAntiAliased: return NONANTIALIASED_QUALITY;
            case FontDesc::AntiAliased:    return ANTIALIASED_QUALITY;
            case FontDesc::ClearType:      return CLEARTYPE_QUALITY;
            case FontDesc::Default:
            default:                       return DEFAULT_QUALITY;
        }
    }

    class FontWin32 : public FontImpl {
    public:
        explicit FontWin32(const FontDesc& desc)
            : m_Desc(desc), m_Font(nullptr)
        {
            // 若指定了自定义字体文件，先私有加载（不污染系统字体表）
            if (!m_Desc.filePath.empty()) {
                m_Loaded = ::AddFontResourceExA(
                    m_Desc.filePath.c_str(), FR_PRIVATE, 0) > 0;
            }

            LOGFONTW lf = { 0 };
            // 字节数：宽字符族名拷贝（Windows API 需 LPWSTR，宏展开前避免中文字面量问题）
            std::wstring wfamily = Utf8ToWide(m_Desc.family);
            if (wfamily.size() >= (size_t)LF_FACESIZE)
                wfamily.resize(LF_FACESIZE - 1);
            wcscpy_s(lf.lfFaceName, wfamily.c_str());

            // 负值 = 以像素为单位（与窗口/Canvas 像素坐标一致，避免 DPI 混淆）
            lf.lfHeight = -m_Desc.size;
            lf.lfWeight = m_Desc.bold ? FW_BOLD : FW_NORMAL;
            lf.lfQuality = ToGdiQuality(m_Desc.quality);
            lf.lfCharSet = DEFAULT_CHARSET;
            // 关键：ClearType 只在不透明背景时生效；我们绘制用 TRANSPARENT 背景，
            // 需配合 CLEARTYPE 的兼容设置，见 EnsureClearTypeCompatible。
            lf.lfOutPrecision = OUT_TT_PRECIS;

            m_Font = ::CreateFontIndirectW(&lf);
            if (!m_Font)
                m_Font = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
        }

        ~FontWin32() override {
            if (m_Font && m_Font != ::GetStockObject(DEFAULT_GUI_FONT)) {
                ::DeleteObject(m_Font);
            }
            if (m_Loaded) {
                // 释放私有加载的自定义字体
                ::RemoveFontResourceExA(m_Desc.filePath.c_str(), FR_PRIVATE, 0);
            }
        }

        const FontDesc& GetDesc() const override { return m_Desc; }

        void* GetNativeHandle() const override { return (void*)m_Font; }

        int GetHeight() const override {
            HDC dc = ::GetDC(nullptr);
            HFONT old = (HFONT)::SelectObject(dc, m_Font);
            TEXTMETRICW tm;
            ::GetTextMetricsW(dc, &tm);
            int h = tm.tmHeight + tm.tmExternalLeading;
            ::SelectObject(dc, old);
            ::ReleaseDC(nullptr, dc);
            return h;
        }

        int GetAscent() const override {
            HDC dc = ::GetDC(nullptr);
            HFONT old = (HFONT)::SelectObject(dc, m_Font);
            TEXTMETRICW tm;
            ::GetTextMetricsW(dc, &tm);
            int a = tm.tmAscent;
            ::SelectObject(dc, old);
            ::ReleaseDC(nullptr, dc);
            return a;
        }

        int GetAvgCharWidth() const override {
            HDC dc = ::GetDC(nullptr);
            HFONT old = (HFONT)::SelectObject(dc, m_Font);
            TEXTMETRICW tm;
            ::GetTextMetricsW(dc, &tm);
            int w = tm.tmAveCharWidth;
            ::SelectObject(dc, old);
            ::ReleaseDC(nullptr, dc);
            return w;
        }

    private:
        // 简单 UTF-8 → UTF-16 转换（字体族名可能含中文，如“微软雅黑”）
        static std::wstring Utf8ToWide(const std::string& s) {
            if (s.empty()) return L"";
            int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                          nullptr, 0);
            std::wstring w;
            if (n > 0) {
                w.resize(n);
                ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                      &w[0], n);
            }
            return w;
        }

        FontDesc        m_Desc;
        HFONT           m_Font = nullptr;
        bool            m_Loaded = false;   // 是否私有加载了自定义字体
    };

    // 工厂实现
    FontImpl* FontFactory::Create(const FontDesc& desc) {
        return new FontWin32(desc);
    }

} // namespace X_Y
