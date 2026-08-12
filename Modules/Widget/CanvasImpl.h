#pragma once
#include <cstdint>

namespace X_Y {

    // 2D 画布抽象——纯虚接口，隐藏平台绘制实现（软件 ARGB / GDI / 其它后端均可）。
    // 职责：**只做填充图形**（矩形/圆角矩形/三角形/裁剪/清屏/双缓冲上屏）。
    // ⚠️ 文字绘制不在此接口。文字能力归 Font（见 Font.h / FontImpl）。
    // ⚠️ 颜色统一 32bpp ARGB（uint32_t）：0xAARRGGBB。alpha 由各后端自行处理。

    class CanvasImpl {
    public:
        virtual ~CanvasImpl() = default;

        // 逻辑尺寸（DPI 缩放后，供布局/命中测试）
        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;

        // 物理像素尺寸(真实位图/窗口大小)。供整幅清屏/填充需覆盖全部物理像素的场景。
        virtual int GetPhysicalWidth() const = 0;
        virtual int GetPhysicalHeight() const = 0;

        // 双缓冲：把内存中已画好的一帧一次性上屏
        virtual void Flush() = 0;

        // 整幅清屏填色（用物理尺寸填满，带 alpha）。常驻画布每帧开头调用。
        virtual void Clear(uint32_t color) = 0;

        // 填充实心矩形（逻辑坐标）
        virtual void FillRect(int x, int y, int w, int h, uint32_t color) = 0;

        // 圆角矩形填充（r=圆角半径像素，逻辑坐标）
        virtual void FillRoundRect(int x, int y, int w, int h,
            int r, uint32_t color) = 0;

        // 填充三角形（三个顶点，逻辑坐标）。用于旋转箭头等自绘形状：
        // 上层先把 3 个点按需旋转/平移，再整体传入。
        virtual void FillTriangle(int x1, int y1, int x2, int y2,
            int x3, int y3, uint32_t color) = 0;

        virtual void SetClip(int x, int y, int w, int h) = 0;
        virtual void ResetClip() = 0;

        // ── 软件 ARGB 缓冲访问（供 Font 文字绘制拼目标 / 特殊上层直读）──
        // 32bpp ARGB 物理像素指针（布局为 width*height，每像素 0xAARRGGBB）。
        // 后端若不暴露（非软件后端）返回 nullptr。
        virtual uint32_t* GetPixelBuffer() = 0;

        // GDI 桥梁 DC（可选）：软件后端创建的、与像素缓冲共享内存的 memory DC，
        // 供 GDI 文字后端 TextOut 直写（alpha 截断）。非 GDI 后端返回 nullptr。
        virtual void* GetBridgeDC() = 0;
    };

    // ── 文字目标 ──
    // 描述"把字画到哪"：程序集软件 ARGB 缓冲 + 尺寸 + 可选 GDI 桥梁 DC。
    // Font::DrawText/FillText 用它拼目标。后端自由选择：FreeType 写 pixels；
    // GDI 经 dc (=bridgeDC) 画（alpha 截断）。POD，无逻辑。
    struct CanvasTarget {
        uint32_t* pixels = nullptr;  // ARGB 像素缓冲（物理尺寸）
        int  width  = 0;             // 物理宽
        int  height = 0;             // 物理高
        void* dc = nullptr;          // GDI 桥梁 DC（可选）
    };

    // 平台工厂
    class CanvasFactory {
    public:
        static CanvasImpl* CreateCanvasImpl(int w, int h, void* nativeHandle);
    };

} // namespace X_Y
