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

        // ============================================================
        // 文字渲染背景策略预留（别写死，将来别瞎加接口）
        //   文字就两种场景；若将来又冒出第三种，先停下来想清楚，
        //   别造新参数/新函数：
        //   场景1「要一块背景」→ 用 FillText：填底 + 写字，
        //                       底是啥不用管，反正都是我们填。
        //   场景2「只写字」   → 将来写在任意位置(图片/渐变/已有内容)
        //                       之上，需要读取底层真实背景给
        //                       ClearType，到时再补读底层背景的接口，
        //                       现在不做。
        //   禁止事项：不搞"全局默认背景色"状态、不做 TextBg
        //   枚举策略、不上全能 DrawTextEx，两个场景对应两个函数就够。
        // ============================================================

        void FillText(int x, int y, int w, int h, int tx, int ty,
            const char* text, uint32_t textColor, uint32_t bgColor,
            const Font* font = nullptr) {
            if (font)
                m_Impl->SetFont(*font);
            m_Impl->FillText(x, y, w, h, tx, ty, text, textColor,
                bgColor);
        }
        void FillText(int x, int y, int w, int h, int tx, int ty,
            const wchar_t* text, uint32_t textColor, uint32_t
            bgColor, const Font* font = nullptr) {
            if (font)
                m_Impl->SetFont(*font);
            m_Impl->FillText(x, y, w, h, tx, ty, text, textColor,
                bgColor);
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
