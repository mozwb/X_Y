#include "../Docklayout/toplayout.h"
namespace X_Y
{
    TopLayout::TopLayout()
    {
        // ① 先把五个区域 Dock 造出来（堆对象）。
        //    ⚠️ 所有权交给基类 DockLayout（DockBind → AddDock 内部用 unique_ptr
        //       收纳）；m_TopDock 等只是【借用视图】，这里不负责 delete ——
        //       它们随基类一起生灭。
        //    这五个是【无宗 Dock】（DockFather == nullptr）：初始区域划分，
        //    相对位置固定，不参与宗族归还。
        m_TopDock = new TabDock();
        m_BottomDock = new TabDock();
        m_LeftDock = new TabDock();
        m_CenterDock = new TabDock();
        m_RightDock = new TabDock();

        // ② 四条内部分割线；外边缘一律用 InvalidBoundary。
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

        m_TopDock->SetMinimumHeight(0.0f);
        m_BottomDock->SetMinimumHeight(0.0f);
        m_LeftDock->SetMinimumWidth(0.0f);
        m_LeftDock->SetMinimumHeight(0.0f);
        m_CenterDock->SetMinimumWidth(0.5f);
        m_CenterDock->SetMinimumHeight(0.05f);
        m_RightDock->SetMinimumWidth(0.0f);
        m_RightDock->SetMinimumHeight(0.0f);

        // ③ 入布局：DockBind 走 AddDock，接管所有权 + 设好四条边界。
        DockBind(*m_TopDock, X_Y::InvalidBoundary, m_TopLine,
                 X_Y::InvalidBoundary, X_Y::InvalidBoundary);
        DockBind(*m_BottomDock, m_BottomLine, X_Y::InvalidBoundary,
                 m_LeftLine, m_RightLine);
        DockBind(*m_LeftDock, m_TopLine, X_Y::InvalidBoundary,
                 X_Y::InvalidBoundary, m_LeftLine);
        DockBind(*m_CenterDock, m_TopLine, m_BottomLine,
                 m_LeftLine, m_RightLine);
        DockBind(*m_RightDock, m_TopLine, X_Y::InvalidBoundary,
                 m_RightLine, X_Y::InvalidBoundary);
    }

} // namespace X_Y
