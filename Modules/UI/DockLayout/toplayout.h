#pragma once
#include "UI/DockLayout/DockLayout.h"
#include "UI/dock/tabdock.h"

//  ____________
// |____________|
// | |        | |
// | |________| |
// |_|________|_|
namespace X_Y
{
    // 五区域标准布局：Top / Bottom / Left / Center / Right。
    //
    // ⚠️ 五个 Dock 必须是 TabDock（不是裸 Dock）：
    //    TabDock 构造函数里 SetMenuBarHeight(30) 才有 tab 栏。
    //    裸 Dock 的 m_MenuBarHeight = 0 → 面板区从 y=0 开始、tab 栏高度 0 画不出来，
    //    表现为"拖进 Dock 后面板盖住 tab 栏位置 / 看不到 tab 栏"。
    class TopLayout : public X_Y::DockLayout
    {
    public:
        TopLayout();
        X_Y::Dock &TopDock() { return m_TopDock; }
        X_Y::Dock &BottomDock() { return m_BottomDock; }
        X_Y::Dock &LeftDock() { return m_LeftDock; }
        X_Y::Dock &CenterDock() { return m_CenterDock; }
        X_Y::Dock &RightDock() { return m_RightDock; }

    private:
        X_Y::BoundaryId m_TopLine = X_Y::InvalidBoundary;
        X_Y::BoundaryId m_BottomLine = X_Y::InvalidBoundary;
        X_Y::BoundaryId m_LeftLine = X_Y::InvalidBoundary;
        X_Y::BoundaryId m_RightLine = X_Y::InvalidBoundary;

        X_Y::TabDock m_TopDock;
        X_Y::TabDock m_BottomDock;
        X_Y::TabDock m_LeftDock;
        X_Y::TabDock m_CenterDock;
        X_Y::TabDock m_RightDock;
    };
}