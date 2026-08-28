#include "../dock/Dock.h"
#include "DockLayout/DockLayout.h"
#include <algorithm>

namespace X_Y
{

    Dock::~Dock()
    {
        m_Panels.clear();
        m_Titles.clear();
    }

    // ── 布局：由自己的边界槽 + 布局总尺寸换算位置矩形 ──
    void Dock::SetActiveRect(int w, int h)
    {
        m_LayoutW = w;
        m_LayoutH = h;
        RecalcRect();
    }

    void Dock::RecalcRect()
    {
        if (!m_Layout)
            return;

        auto halfGap = [&](BoundaryId id) -> int
        {
            const Boundary *b = m_Layout->GetBoundary(id);
            return b ? b->width / 2 : 0;
        };
        auto sidePix = [&](BoundaryId id, bool isRight)->int
        {
            const Boundary *b = m_Layout->GetBoundary(id);
            if (b)
                return m_Layout->GetBoundaryPosition(id);
            return isRight ? m_LayoutW : 0;
        };

        const int left = sidePix(m_Boundary.left, false) + halfGap(m_Boundary.left);
        const int right = sidePix(m_Boundary.right, true) - halfGap(m_Boundary.right);
        const int top = sidePix(m_Boundary.top, false) + halfGap(m_Boundary.top);
        const int bottom = sidePix(m_Boundary.bottom, true) - halfGap(m_Boundary.bottom);

        if (right >= left && bottom >= top)
        {
            m_X = left;
            m_Y = top;
            m_W = right - left;
            m_H = bottom - top;
        }
    }

    // ── 边界命中（供拖分割线；布局坐标）──
    BoundaryId Dock::HitTestEdge(int x, int y, int thickness) const
    {
        if (!m_Layout)
            return InvalidBoundary;

        auto hitSide = [&](BoundaryId id, bool vertical) -> bool
        {
            if (id == InvalidBoundary)
                return false;
            const Boundary *b = m_Layout->GetBoundary(id);
            if (!b)
                return false;
            const int pos = m_Layout->GetBoundaryPosition(id);
            const int hitT = std::max(thickness, b->width / 2);
            const int coord = vertical ? x : y;
            if (std::abs(coord - pos) > hitT)
                return false;
            // 跨度沿另一维度
            const int extent = vertical ? m_LayoutH : m_LayoutW;
            const int spanLo = static_cast<int>(b->start * extent);
            const int spanHi = static_cast<int>(b->end * extent);
            const int spanCoord = vertical ? y : x;
            return spanCoord >= spanLo && spanCoord <= spanHi;
        };

        if (hitSide(m_Boundary.left, true))  return m_Boundary.left;
        if (hitSide(m_Boundary.right, true)) return m_Boundary.right;
        if (hitSide(m_Boundary.top, false))  return m_Boundary.top;
        if (hitSide(m_Boundary.bottom, false)) return m_Boundary.bottom;
        return InvalidBoundary;
    }

    Panel *Dock::HitTestPanel(int x, int y) const
    {
        if (m_ActiveIndex < 0 || m_ActiveIndex >= (int)m_Panels.size())
            return nullptr;
        Panel *p = m_Panels[m_ActiveIndex];
        if (p)
        {
            const int px = p->GetX(), py = p->GetY();
            const int pw = p->GetWidth(), ph = p->GetHeight();
            if (x >= px && x < px + pw && y >= py && y < py + ph)
                return p;
        }
        return nullptr;
    }

    // ── 面板管理 ──
    Panel *Dock::AddPanel(Panel *panel, const std::string &title)
    {
        if (!panel)
            return nullptr;
        m_Panels.push_back(panel);
        m_Titles.push_back(title);
        m_ActiveIndex = (int)m_Panels.size() - 1;
        if (m_Layout)
            m_Layout->RecalcLayout();
        RequestRepaint();
        return panel;
    }

    bool Dock::RemovePanel(Panel *panel)
    {
        auto it = std::find(m_Panels.begin(), m_Panels.end(), panel);
        if (it == m_Panels.end())
            return false;
        const int idx = (int)(it - m_Panels.begin());
        m_Panels.erase(it);
        m_Titles.erase(m_Titles.begin() + idx);
        if (m_ActiveIndex >= (int)m_Panels.size())
            m_ActiveIndex = (int)m_Panels.size() - 1;
        if (m_Panels.empty())
            m_ActiveIndex = -1;
        RequestRepaint();
        return true;
    }

    void Dock::ActivatePanel(int idx)
    {
        if (idx < 0 || idx >= (int)m_Panels.size())
            return;
        if (m_ActiveIndex == idx)
            return;
        m_ActiveIndex = idx;
        ShowActivePanel();
        RequestRepaint();
    }

    bool Dock::ActivatePanel(Panel *panel)
    {
        for (int i = 0; i < (int)m_Panels.size(); ++i)
            if (m_Panels[i] == panel)
            {
                ActivatePanel(i);
                return true;
            }
        return false;
    }

    Panel *Dock::GetActivePanel() const
    {
        if (m_ActiveIndex < 0 || m_ActiveIndex >= (int)m_Panels.size())
            return nullptr;
        return m_Panels[m_ActiveIndex];
    }

    Panel *Dock::GetPanel(int idx) const
    {
        if (idx < 0 || idx >= (int)m_Panels.size())
            return nullptr;
        return m_Panels[idx];
    }

    const std::string &Dock::GetPanelTitle(int idx) const
    {
        static const std::string s_Empty;
        if (idx < 0 || idx >= (int)m_Titles.size())
            return s_Empty;
        return m_Titles[idx];
    }

    void Dock::ShowActivePanel()
    {
        // 让激活面板占据本 Dock 的内容区（去掉 tab 栏高度）
        const int contentY = m_Y + kTabBarHeight;
        const int contentH = std::max(0, m_H - kTabBarHeight);
        Panel *active = GetActivePanel();
        if (active)
            active->SetLayoutRect(m_X, contentY, m_W, contentH);
        RequestRepaint();
    }

    // ── 绘制：tab 栏 + 激活 Panel 内容 ──
    void Dock::OnPaint(Canvas &canvas)
    {
        canvas.FillRect(m_X, m_Y, m_W, m_H, 0xFF232527);

        // tab 栏
        const int tabY = m_Y;
        int tabX = m_X;
        for (int i = 0; i < (int)m_Panels.size(); ++i)
        {
            const bool active = (i == m_ActiveIndex);
            const int tabW = 120;
            canvas.FillRect(tabX, tabY, tabW, kTabBarHeight,
                            active ? 0xFF007ACC : 0xFF3E3E42);
            // 用 Panel 首标/序列做标题占位（标题存 m_Titles）
            // (字体绘制后续接 FontLibrary；先只画色块+索引)
            tabX += tabW;
        }

        // 激活 Panel 内容
        Panel *active = GetActivePanel();
        if (active)
            active->OnPaint(canvas);
    }

} // namespace X_Y
