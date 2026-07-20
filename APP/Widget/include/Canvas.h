#pragma once
#include <memory>
#include "CanvasImpl.h"

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // 2D 画布——轻量包装，隐藏平台实现
    class Canvas {
    public:
        Canvas(int w, int h, void* nativeHandle)
            : m_Impl(CanvasFactory::CreateCanvasImpl(w, h,
                nativeHandle))
        {
        }

        int GetWidth() const { return m_Impl->GetWidth(); }
        int GetHeight() const { return m_Impl->GetHeight(); }
 
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
    };

} // namespace X_Y