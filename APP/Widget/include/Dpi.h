#pragma once

namespace X_Y {

    // DPI 缩放工具 —— 全自动，运行时从系统实时读取，无需用户配置。
    // 用法：
    //   1) 程序入口(WinMain/main)最前调 DeclareAware()，告诉系统"本程序
    //      DPI-aware"，消除系统位图拉伸导致的模糊。
    //   2) 之后所有底层(窗口尺寸/Canvas 绘制/字体字号) 用 GetScale()
    //      把逻辑尺寸换算成物理像素。用户改显示缩放后，GetScale()
    //      自动返回新值，程序自适应，无需重启。
    class Dpi {
    public:
        // 启动时调用一次：声明本程序 DPI-aware（调 Win32 接口，失败则
        // 回退 SetProcessDPIAware）。
        static void DeclareAware();

        // 运行时自动读取系统 DPI（用户改缩放即实时更新）
        static int GetDpi();            // 如 96 / 120 / 144
        static float GetScale();        // = GetDpi() / 96.f，如 1.5f
    };

} // namespace X_Y
