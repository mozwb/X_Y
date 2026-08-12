#pragma once
#include <memory>
#include "CanvasImpl.h"
#include "Font.h"

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // 2D 画布——轻量包装，隐藏平台实现。
    // 职责：**只做填充图形 + 双缓冲**。文字不在此持有，文字属于 Font。
    // 文字 API（DrawText/FillText）是**薄转发壳**：把本画布的目标缓冲组装成
    // CanvasTarget 传给 font 真渲染（文字后端选择归 Font，GDI alpha 截断 / FreeType 透明）。

    class Canvas {
    public:
        Canvas(int w, int h, void* nativeHandle)
            : m_Impl(CanvasFactory::CreateCanvasImpl(w, h, nativeHandle))
        {
        }

        int GetWidth() const { return m_Impl->GetWidth(); }
        int GetHeight() const { return m_Impl->GetHeight(); }

        // 物理像素尺寸(真实位图/窗口大小)。与 GetWidth/GetHeight(逻辑,DPI缩放后)区分，
        // 供整幅清屏/填充等必须覆盖全部物理像素的场景使用。
        int GetPhysicalWidth() const { return m_Impl->GetPhysicalWidth(); }
        int GetPhysicalHeight() const { return m_Impl->GetPhysicalHeight(); }

        // 双缓冲：把内存中已画好的一帧一次性上屏
        void Flush() { m_Impl->Flush(); }

        // 整幅清屏填色（物理尺寸，带 alpha）
        void Clear(uint32_t color) { m_Impl->Clear(color); }

        void FillRect(int x, int y, int w, int h, uint32_t color) {
            m_Impl->FillRect(x, y, w, h, color);
        }
        void FillRoundRect(int x, int y, int w, int h, int r, uint32_t color) {
            m_Impl->FillRoundRect(x, y, w, h, r, color);
        }
        void FillTriangle(int x1, int y1, int x2, int y2,
                          int x3, int y3, uint32_t color) {
            m_Impl->FillTriangle(x1, y1, x2, y2, x3, y3, color);
        }

        // ── 文字（薄转发壳，文字逻辑归 Font）──
        // font = "用哪款字体画这段字"（每次显式指定，无 SetFont 状态）。
        void DrawText(const Font& font, int x, int y,
                      const char* text, uint32_t color) {
            font.DrawText(MakeTarget(), x, y, text, color);
        }
        void DrawText(const Font& font, int x, int y,
                      const wchar_t* text, uint32_t color) {
            font.DrawText(MakeTarget(), x, y, text, color);
        }
        void FillText(const Font& font, int x, int y, int w, int h,
                      int tx, int ty, const char* text,
                      uint32_t textColor, uint32_t bgColor) {
            font.FillText(MakeTarget(), x, y, w, h, tx, ty, text, textColor, bgColor);
        }
        void FillText(const Font& font, int x, int y, int w, int h,
                      int tx, int ty, const wchar_t* text,
                      uint32_t textColor, uint32_t bgColor) {
            font.FillText(MakeTarget(), x, y, w, h, tx, ty, text, textColor, bgColor);
        }

        void SetClip(int x, int y, int w, int h) { m_Impl->SetClip(x, y, w, h); }
        void ResetClip() { m_Impl->ResetClip(); }

        // ── 软件 ARGB 缓冲直取（供 Font 拼目标 / 特殊上层直读）──
        uint32_t* GetPixelBuffer() { return m_Impl->GetPixelBuffer(); }
        void* GetBridgeDC() { return m_Impl->GetBridgeDC(); }

    private:
        // 组装文字绘制目标（本画布软件缓冲 + 尺寸 + GDI 桥 DC）
        CanvasTarget MakeTarget() {
            CanvasTarget t;
            t.pixels = m_Impl->GetPixelBuffer();
            t.width  = m_Impl->GetPhysicalWidth();
            t.height = m_Impl->GetPhysicalHeight();
            t.dc     = m_Impl->GetBridgeDC();
            return t;
        }

        std::unique_ptr<CanvasImpl> m_Impl;
    };

} // namespace X_Y
