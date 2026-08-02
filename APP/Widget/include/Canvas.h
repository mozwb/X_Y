#pragma once
#include <memory>
#include "CanvasImpl.h"
#include "Font.h"

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // 2D 画布——轻量包装，隐藏平台实现

    class Canvas {
    public:
        Canvas(int w, int h, void* nativeHandle)
            : m_Impl(CanvasFactory::CreateCanvasImpl(w, h,
                nativeHandle)), m_DefaultFont()
        {
            // 默认字体：微软雅黑 ClearType，让所有 DrawText 立即清晰抗锯齿
            m_Impl->SetFont(m_DefaultFont);
        }

        int GetWidth() const { return m_Impl->GetWidth(); }
        int GetHeight() const { return m_Impl->GetHeight(); }
 
        // 双缓冲：把内存中已画好的一帧一次性上屏

        void Flush() {
            m_Impl->Flush();
        }

        // 设置绘制字体（可传自定义 Font，含自下载的 ttf）
        void SetFont(const Font& font) {
            // 复制描述重建一个（Font 持有 unique_ptr 不可拷，复制 desc 最安全）
            m_DefaultFont = Font(font.GetDesc());
            m_Impl->SetFont(m_DefaultFont);
        }

        void FillRect(int x, int y, int w, int h, uint32_t
            color) {
            m_Impl->FillRect(x, y, w, h, color);
        }
        void DrawText(int x, int y, const char* text, uint32_t
            color) {
            m_Impl->DrawText(x, y, text, color);
        }
        void DrawText(int x, int y, const wchar_t* text,
            uint32_t color) {
            m_Impl->DrawText(x, y, text, color);
        }
        void SetClip(int x, int y, int w, int h) {
            m_Impl->SetClip(x, y, w, h);
        }
        void ResetClip() {
            m_Impl->ResetClip();
        }

    private:
        std::unique_ptr<CanvasImpl> m_Impl;
        Font m_DefaultFont;   // 当前绘制字体（默认微软雅黑 ClearType）
    };

} 
// namespace X_Y
