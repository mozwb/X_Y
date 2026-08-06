#pragma once
#include <cstdint>
#include "Widget/include/Font.h"

#ifdef DrawText
#undef DrawText
#endif

namespace X_Y {

    // 2D 绘制接口——纯虚，不依赖任何平台类型
    class CanvasImpl {
    public:
        virtual ~CanvasImpl() = default;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;

        // 双缓冲：把内存中已画好的一帧一次性上屏
        virtual void Flush() = 0;

        // 设置当前绘制字体（后续 DrawText 采用该字体）
        virtual void SetFont(const Font& font) = 0;

        virtual void FillRect(int x, int y, int w, int h, uint32_t
            color) = 0;
        virtual void DrawText(int x, int y, const char* text,
            uint32_t color) = 0;
        virtual void DrawText(int x, int y, const wchar_t* text,
            uint32_t color) = 0;

        // 组合拳：铺背景 + 写字（背景一次传入，ClearType 用同一背景色）
        // (x,y,w,h)=背景矩形；(tx,ty)=文字起点，可与背景错开(缩进/偏移)。
        // 窄/宽字符两版，窄版在平台实现层转宽。
        virtual void FillText(int x, int y, int w, int h, int tx,
            int ty, const char* text, uint32_t textColor, uint32_t
            bgColor) = 0;
        virtual void FillText(int x, int y, int w, int h, int tx,
            int ty, const wchar_t* text, uint32_t textColor,
            uint32_t bgColor) = 0;

        // 测量文字宽度（用当前已设置字体），返回逻辑像素宽。
        // 供折行/布局判断文字是否超出可用宽度。

        virtual int MeasureText(const wchar_t* text) = 0;
        virtual int MeasureText(const char* text) = 0;
        virtual void SetClip(int x, int y, int w, int h) = 0;
        virtual void ResetClip() = 0;
    };

    // 平台工厂
    class CanvasFactory {
    public:
        static CanvasImpl* CreateCanvasImpl(int w, int h, void*
            nativeHandle);
    };

} // namespace X_Y