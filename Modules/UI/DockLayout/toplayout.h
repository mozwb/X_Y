#pragma once
#include "UI/DockLayout/DockLayout.h"

//  ____________
// |____________|
// | |        | |
// | |________| |
// |_|________|_|
namespace X_Y
{
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

        X_Y::Dock m_TopDock;
        X_Y::Dock m_BottomDock;
        X_Y::Dock m_LeftDock;
        X_Y::Dock m_CenterDock;
        X_Y::Dock m_RightDock;
    };
}