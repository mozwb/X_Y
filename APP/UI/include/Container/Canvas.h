#pragma once
#include <cstdint>

#ifdef XY_PLATFORM_WINDOWS
#include <windows.h>
typedef HDC CanvasHandle;
#else
#error "不支持其他操作系统"
#endif

namespace X_Y {

    class Canvas {
    public:
        Canvas(int w, int h, CanvasHandle handle);

        int GetWidth() const;
        int GetHeight() const;

        void FillRect(int x, int y, int w, int h, uint32_t color);
        void DrawText(int x, int y, const char* text, uint32_t color);
        void DrawText(int x, int y, const wchar_t* text, uint32_t color);
        void SetClip(int x, int y, int w, int h);
        void ResetClip();

    private:
        CanvasHandle m_Handle;
        int m_Width, m_Height;
    };

}
