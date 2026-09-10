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
        TopLayout()
        {
            // Four internal split lines; the outer edges use InvalidBoundary.
            m_TopLine = AddBoundary(X_Y::BoundaryOrientation::Horizontal, 0.20f);
            m_BottomLine = AddBoundary(X_Y::BoundaryOrientation::Horizontal, 0.80f);
            m_LeftLine = AddBoundary(X_Y::BoundaryOrientation::Vertical, 0.20f);
            m_RightLine = AddBoundary(X_Y::BoundaryOrientation::Vertical, 0.80f);

            SetBoundaryColor(m_TopLine, 0xFF4FC3F7);
            SetBoundaryColor(m_BottomLine, 0xFFFFB74D);
            SetBoundaryColor(m_LeftLine, 0xFF81C784);
            SetBoundaryColor(m_RightLine, 0xFFE57373);

            SetBoundaryRange(m_TopLine, 0.0f, 1.0f);
            SetBoundaryRange(m_BottomLine, 0.0f, 1.0f);
            SetBoundaryRange(m_LeftLine, 0.0f, 1.0f);
            SetBoundaryRange(m_RightLine, 0.0f, 1.0f);

            SetBoundarySize(m_LeftLine, m_TopLine, X_Y::InvalidBoundary);
            SetBoundarySize(m_RightLine, m_TopLine, X_Y::InvalidBoundary);
            SetBoundarySize(m_BottomLine, m_LeftLine, m_RightLine);

            m_TopDock.SetMinimumHeight(0.0f);
            m_BottomDock.SetMinimumHeight(0.0f);
            m_LeftDock.SetMinimumWidth(0.0f);
            m_LeftDock.SetMinimumHeight(0.0f);
            m_CenterDock.SetMinimumWidth(0.5f);
            m_CenterDock.SetMinimumHeight(0.05f);
            m_RightDock.SetMinimumWidth(0.0f);
            m_RightDock.SetMinimumHeight(0.0f);

            DockBind(m_TopDock, X_Y::InvalidBoundary, m_TopLine,
                     X_Y::InvalidBoundary, X_Y::InvalidBoundary);
            DockBind(m_BottomDock, m_BottomLine, X_Y::InvalidBoundary,
                     m_LeftLine, m_RightLine);
            DockBind(m_LeftDock, m_TopLine, X_Y::InvalidBoundary,
                     X_Y::InvalidBoundary, m_LeftLine);
            DockBind(m_CenterDock, m_TopLine, m_BottomLine,
                     m_LeftLine, m_RightLine);
            DockBind(m_RightDock, m_TopLine, X_Y::InvalidBoundary,
                     m_RightLine, X_Y::InvalidBoundary);
        }

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