#pragma once
#include <cstdint>

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

        virtual void FillRect(int x, int y, int w, int h, uint32_t
            color) = 0;
        virtual void DrawText(int x, int y, const char* text,
            uint32_t color) = 0;
        virtual void DrawText(int x, int y, const wchar_t* text,
            uint32_t color) = 0;
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