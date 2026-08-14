#include "CanvasImpl.h"
#include "Dpi.h"
#include <windows.h>
#include <cstring>

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y
{

    // ── CanvasImpl 软件 ARGB 后端 ──────────────────────────
    // 图形：直接往 32bpp ARGB 像素缓冲软件光栅化（支持 alpha 合成）。
    // 文字：GDI 后端经桥梁 DC（共享同一 DIB）TextOut 直写（alpha 截断）；
    //       FreeType 后端(未来)直接写 pixels。
    // 上屏：普通窗 BitBlt(忽略 alpha)；Layered 窗 UpdateLayeredWindow(alpha 生效)。
    // ⚠️ 逻辑/物理：逻辑坐标(布局) × DPI scale = 物理像素(缓冲)。

    class CanvasImplSoftware : public CanvasImpl
    {
    public:
        CanvasImplSoftware(int w, int h, HWND hwnd)
            : m_Width(w), m_Height(h), m_Hwnd(hwnd),
              m_Pixels(nullptr), m_DIB(nullptr), m_DC(nullptr), m_OldBitmap(nullptr),
              m_ClipSet(false)
        {
            m_Scale = Dpi::GetScale();
            m_LogicW = (int)(w / m_Scale);
            m_LogicH = (int)(h / m_Scale);
            if (m_LogicW < 1)
                m_LogicW = 1;
            if (m_LogicH < 1)
                m_LogicH = 1;

            // 创建 32bpp ARGB DIB 段（同时拿像素指针 + 关联 DC 作 GDI 桥梁）
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h; // 负 = 自顶向下（与内存布局一致）
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            m_DIB = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                       (void **)&m_Pixels, nullptr, 0);
            if (m_DIB)
            {
                // 清一次零，避免垃圾像素
                if (m_Pixels)
                    ::memset(m_Pixels, 0, (size_t)w * h * 4);
                m_DC = ::CreateCompatibleDC(nullptr);
                if (m_DC)
                    m_OldBitmap = (HBITMAP)::SelectObject(m_DC, m_DIB);
            }
        }

        ~CanvasImplSoftware() override
        {
            if (m_DC && m_OldBitmap)
                ::SelectObject(m_DC, m_OldBitmap);
            if (m_DC)
                ::DeleteDC(m_DC);
            if (m_DIB)
                ::DeleteObject(m_DIB);
        }

        int GetWidth() const override { return m_LogicW; }
        int GetHeight() const override { return m_LogicH; }
        int GetPhysicalWidth() const override { return m_Width; }
        int GetPhysicalHeight() const override { return m_Height; }

        // 逻辑 ↔ 物理 互译（基于后端自持 m_Scale，与 S() 同一份换算）
        float GetScale() const override { return m_Scale; }
        int LToP(int logical) const override { return S(logical); }
        int PToL(int physical) const override
        {
            return (int)(physical / m_Scale + 0.5f);
        }

        uint32_t *GetPixelBuffer() override { return m_Pixels; }
        void *GetBridgeDC() override { return m_DC; }

        void Flush() override
        {
            if (!m_Pixels)
                return;
            HDC target = m_Hwnd ? ::GetDC(m_Hwnd) : ::GetDC(nullptr);
            if (!target)
                return;

            bool layered = m_Hwnd &&
                           (::GetWindowLongPtrW(m_Hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) != 0;

            if (layered)
            {
                // Layered 窗口：per-pixel alpha（DIB 已是自顶向下 ARGB）
                BLENDFUNCTION bf = {};
                bf.BlendOp = AC_SRC_OVER;
                bf.SourceConstantAlpha = 255;
                bf.AlphaFormat = AC_SRC_ALPHA;
                POINT ptSrc = {0, 0};
                SIZE size = {m_Width, m_Height};
                POINT ptDst = {0, 0};
                ::UpdateLayeredWindow(m_Hwnd, nullptr, &ptDst, &size,
                                      m_DC, &ptSrc, 0, &bf, ULW_ALPHA);
            }
            else
            {
                // 普通窗口：BitBlt（忽略 alpha，显示 RGB）
                ::BitBlt(target, 0, 0, m_Width, m_Height,
                         m_DC, 0, 0, SRCCOPY);
            }

            if (m_Hwnd)
                ::ReleaseDC(m_Hwnd, target);
            else
                ::ReleaseDC(nullptr, target);
        }

        void Clear(uint32_t color) override
        {
            if (!m_Pixels)
                return;
            // 整幅填（物理尺寸），含 alpha。逐像素覆盖，不合成。
            uint32_t c = ToARGBPremult(color);
            uint32_t *p = m_Pixels;
            uint32_t n = (uint32_t)m_Width * (uint32_t)m_Height;
            for (uint32_t i = 0; i < n; ++i)
                *p++ = c;
        }

        void FillRect(int x, int y, int w, int h, uint32_t color) override
        {
            int px = S(x), py = S(y), pw = S(w), ph = S(h);
            FillRectPhys(px, py, pw, ph, color);
        }

        void FillRoundRect(int x, int y, int w, int h, int r, uint32_t color) override
        {
            if (r < 0)
                r = 0;
            int px = S(x), py = S(y), pw = S(w), ph = S(h);
            int pr = S(r);
            if (pw <= 0 || ph <= 0)
                return;
            // 软件画圆角：逐行判断 4 个圆角象限，落在圆角外则跳过。
            // 拆成矩形(中心区)+四角(逐像素判距)简单起见，这里直接整块逐像素判距。
            uint32_t c = ToARGBPremult(color);
            for (int yy = 0; yy < ph; ++yy)
            {
                for (int xx = 0; xx < pw; ++xx)
                {
                    int cx = xx, cy = yy;
                    // 距离最近角
                    int dx = 0, dy = 0;
                    if (cx < pr && cy < pr)
                    {
                        dx = cx;
                        dy = cy;
                    } // 左上
                    else if (cx >= pw - pr && cy < pr)
                    {
                        dx = pw - 1 - cx;
                        dy = cy;
                    } // 右上
                    else if (cx < pr && cy >= ph - pr)
                    {
                        dx = cx;
                        dy = ph - 1 - cy;
                    } // 左下
                    else if (cx >= pw - pr && cy >= ph - pr)
                    {
                        dx = pw - 1 - cx;
                        dy = ph - 1 - cy;
                    } // 右下
                    else
                    {
                        dx = -1;
                    }
                    bool inside;
                    if (dx < 0)
                        inside = true; // 非角落区域
                    else
                        inside = (dx * dx + dy * dy) <= pr * pr;
                    if (!inside)
                        continue;
                    BlitPixel(px + xx, py + yy, c);
                }
            }
        }

        void FillTriangle(int x1, int y1, int x2, int y2,
                          int x3, int y3, uint32_t color) override
        {
            int ax = S(x1), ay = S(y1);
            int bx = S(x2), by = S(y2);
            int cx = S(x3), cy = S(y3);
            // 包围盒
            int minX = min3(ax, bx, cx), maxX = max3(ax, bx, cx);
            int minY = min3(ay, by, cy), maxY = max3(ay, by, cy);
            uint32_t c = ToARGBPremult(color);
            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    if (PointInTri(x, y, ax, ay, bx, by, cx, cy))
                        BlitPixel(x, y, c);
                }
            }
        }

        void SetClip(int x, int y, int w, int h) override
        {
            m_ClipX = S(x);
            m_ClipY = S(y);
            m_ClipW = S(w);
            m_ClipH = S(h);
            m_ClipSet = true;
        }

        void ResetClip() override { m_ClipSet = false; }

    private:
        void FillRectPhys(int x, int y, int w, int h, uint32_t color)
        {
            if (w <= 0 || h <= 0)
                return;
            uint32_t c = ToARGBPremult(color);
            for (int yy = 0; yy < h; ++yy)
                for (int xx = 0; xx < w; ++xx)
                    BlitPixel(x + xx, y + yy, c);
        }

        // 写一像素（含裁剪 + 越界 + alpha 合成）
        void BlitPixel(int x, int y, uint32_t src)
        {
            if (m_ClipSet &&
                (x < m_ClipX || x >= m_ClipX + m_ClipW ||
                 y < m_ClipY || y >= m_ClipY + m_ClipH))
                return;
            if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
                return;
            uint32_t *d = &m_Pixels[(size_t)y * m_Width + x];
            // 源 alpha
            uint32_t sa = (src >> 24) & 0xFF;
            if (sa >= 255)
            {
                *d = src;
                return;
            }
            if (sa == 0)
            {
                return;
            }
            // alpha 合成（源已 premult）
            uint32_t da = (*d >> 24) & 0xFF;
            uint32_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
            uint32_t dr = (*d >> 16) & 0xFF, dg = (*d >> 8) & 0xFF, db = *d & 0xFF;
            uint32_t oa = sa + da * (255 - sa) / 255;
            uint32_t r = (sa * sr + (255 - sa) * dr) / 255;
            uint32_t g = (sa * sg + (255 - sa) * dg) / 255;
            uint32_t b = (sa * sb + (255 - sa) * db) / 255;
            *d = (oa << 24) | (r << 16) | (g << 8) | b;
        }

        // 0xAARRGGBB → premultiplied（Flush 给 UpdateLayeredWindow；软件合成用）
        static uint32_t ToARGBPremult(uint32_t c)
        {
            uint32_t a = (c >> 24) & 0xFF;
            uint32_t r = ((c >> 16) & 0xFF) * a / 255;
            uint32_t g = ((c >> 8) & 0xFF) * a / 255;
            uint32_t b = (c & 0xFF) * a / 255;
            return (a << 24) | (r << 16) | (g << 8) | b;
        }

        static int min3(int a, int b, int c) { return a < b ? (a < c ? a : c) : (b < c ? b : c); }
        static int max3(int a, int b, int c) { return a > b ? (a > c ? a : c) : (b > c ? b : c); }

        // 点在三角形内（重心坐标，含边界）
        static bool PointInTri(int px, int py,
                               int x1, int y1, int x2, int y2, int x3, int y3)
        {
            auto sign = [](int p1x, int p1y, int p2x, int p2y, int p3x, int p3y) -> int
            {
                return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
            };
            int d1 = sign(px, py, x1, y1, x2, y2);
            int d2 = sign(px, py, x2, y2, x3, y3);
            int d3 = sign(px, py, x3, y3, x1, y1);
            bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            return !(neg && pos);
        }

        int S(int v) const { return (int)(v * m_Scale + 0.5f); }

        int m_Width, m_Height;  // 物理像素
        int m_LogicW, m_LogicH; // 逻辑尺寸
        float m_Scale = 1.0f;
        HWND m_Hwnd = nullptr;
        uint32_t *m_Pixels = nullptr;
        HBITMAP m_DIB = nullptr;
        HDC m_DC = nullptr; // GDI 桥梁 DC（共享 DIB 像素）
        HBITMAP m_OldBitmap = nullptr;
        bool m_ClipSet = false;
        int m_ClipX = 0, m_ClipY = 0, m_ClipW = 0, m_ClipH = 0;
    };

    // 工厂：nativeHandle = 窗口句柄(HWND)。创建软件 ARGB 后端。
    CanvasImpl *CanvasFactory::CreateCanvasImpl(int w, int h, void *nativeHandle)
    {
        return new CanvasImplSoftware(w, h, (HWND)nativeHandle);
    }

} // namespace X_Y
