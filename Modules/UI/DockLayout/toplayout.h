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
    //
    // ── 所有权（见 DockLayout 文件头说明）──
    //    TopLayout 是 DockLayout 的便利子类，【它就是这个窗口的 layout】。
    //    五个区域 Dock 由本布局拥有（基类的 m_OwnedDocks 里的 unique_ptr），
    //    这里的 m_TopDock 等只是【借用视图】—— 方便按名字取用（TopDock()），
    //    绝不负责释放；它们随基类一起生灭。
    //
    //    这五个是【无宗 Dock】（DockFather == nullptr）：窗口最初始的区域划分，
    //    相对位置固定，不参与宗族归还。
    class TopLayout : public X_Y::DockLayout
    {
    public:
        TopLayout();
        ~TopLayout() override = default;

        X_Y::Dock &TopDock() { return *m_TopDock; }
        X_Y::Dock &BottomDock() { return *m_BottomDock; }
        X_Y::Dock &LeftDock() { return *m_LeftDock; }
        X_Y::Dock &CenterDock() { return *m_CenterDock; }
        X_Y::Dock &RightDock() { return *m_RightDock; }

    private:
        X_Y::BoundaryId m_TopLine = X_Y::InvalidBoundary;
        X_Y::BoundaryId m_BottomLine = X_Y::InvalidBoundary;
        X_Y::BoundaryId m_LeftLine = X_Y::InvalidBoundary;
        X_Y::BoundaryId m_RightLine = X_Y::InvalidBoundary;

        // ⚠️ 借用视图：不拥有（基类的 m_OwnedDocks 才是主人）
        X_Y::TabDock *m_TopDock = nullptr;
        X_Y::TabDock *m_BottomDock = nullptr;
        X_Y::TabDock *m_LeftDock = nullptr;
        X_Y::TabDock *m_CenterDock = nullptr;
        X_Y::TabDock *m_RightDock = nullptr;
    };
}
