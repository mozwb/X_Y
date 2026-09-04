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
        if (!m_PanelAreaCustomized)
        {
            m_PanelX = 0;
            m_PanelY = m_MenuBarHeight;
            m_PanelW = m_W;
            m_PanelH = std::max(0, m_H - m_MenuBarHeight);
        }
        ShowActivePanel(); // 重排后同步激活 Panel 的布局矩形
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
        auto sidePix = [&](BoundaryId id, bool isRight) -> int
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

    void Dock::SetPanelArea(int x, int y, int w, int h)
    {
        m_PanelX = x;
        m_PanelY = y;
        m_PanelW = std::max(0, w);
        m_PanelH = std::max(0, h);
        m_PanelAreaCustomized = true;
        ShowActivePanel();
        RequestRepaint();
    }

    void Dock::GetPanelArea(int &x, int &y, int &w, int &h) const
    {
        x = m_PanelX;
        y = m_PanelY;
        w = m_PanelW;
        h = m_PanelH;
    }

    void Dock::SetMenuBarHeight(int height)
    {
        m_MenuBarHeight = std::max(0, height);
        if (!m_PanelAreaCustomized)
        {
            m_PanelY = m_MenuBarHeight;
            m_PanelW = m_W;
            m_PanelH = std::max(0, m_H - m_MenuBarHeight);
        }
        ShowActivePanel();
        RequestRepaint();
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

        if (hitSide(m_Boundary.left, true))
            return m_Boundary.left;
        if (hitSide(m_Boundary.right, true))
            return m_Boundary.right;
        if (hitSide(m_Boundary.top, false))
            return m_Boundary.top;
        if (hitSide(m_Boundary.bottom, false))
            return m_Boundary.bottom;
        return InvalidBoundary;
    }

    Panel *Dock::HitTestPanel(int x, int y) const
    {
        // x/y 为 dock 内坐标(原点 dock 左上)；菜单栏区域不算面板内容
        if (x < m_PanelX || x >= m_PanelX + m_PanelW ||
            y < m_PanelY || y >= m_PanelY + m_PanelH)
            return nullptr;
        Panel *p = GetActivePanel();
        if (!p)
            return nullptr;
        return p;
    }

    // ── 输入：命中 tab 栏切 tab；否则下传给激活 Panel ──
    void Dock::RouteInput(UIInputEvent &e)
    {
        // e.x/e.y 为 dock 内坐标（0,0 = dock 左上，由 DockLayout 平移好）
        if (auto *me = dynamic_cast<UIMouseEvent *>(&e))
        {
            if (me->action == MouseAction::Press && e.y >= 0 && e.y < m_MenuBarHeight)
            {
                const int tabW = 120;
                int x0 = 0;
                const int n = (int)m_Panels.size();
                for (int i = 0; i < n; ++i)
                {
                    if (e.x >= x0 && e.x < x0 + tabW)
                    {
                        if (i != m_ActiveIndex)
                            ActivatePanel(i);
                        e.Handled = true;
                        return;
                    }
                    x0 += tabW;
                }
            }
        }

        // 键盘事件：无坐标意义，直接下传给激活 Panel（Panel 内部走焦点）
        if (dynamic_cast<UIKeyEvent *>(&e))
        {
            Panel *p = GetActivePanel();
            if (p)
                p->OnInput(e);
            return;
        }

        Panel *p = HitTestPanel(e.x, e.y);
        if (p)
        {
            const int px0 = p->GetX() - m_X; // panel 相对 dock 的偏移
            const int py0 = p->GetY() - m_Y;
            e.x -= px0;
            e.y -= py0;
            p->OnInput(e);
            e.x += px0;
            e.y += py0;
        }
    }

    // ── 面板管理 ──
    Panel *Dock::AddPanel(Panel *panel, const std::string &title)
    {
        if (!panel || !CanAddPanel())
            return nullptr;
        panel->SetHostRepaint(m_HostRepaint);
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

    Panel *Dock::DetachPanel(Panel *panel, std::string *title)
    {
        auto it = std::find(m_Panels.begin(), m_Panels.end(), panel);
        if (it == m_Panels.end())
            return nullptr;

        const int idx = static_cast<int>(it - m_Panels.begin());
        if (title)
            *title = m_Titles[idx];

        m_Panels.erase(it);
        m_Titles.erase(m_Titles.begin() + idx);
        if (m_Panels.empty())
            m_ActiveIndex = -1;
        else if (m_ActiveIndex > idx)
            --m_ActiveIndex;
        else if (m_ActiveIndex >= static_cast<int>(m_Panels.size()))
            m_ActiveIndex = static_cast<int>(m_Panels.size()) - 1;

        ShowActivePanel();
        RequestRepaint();
        return panel;
    }

    bool Dock::ContainsPanel(const Panel *panel) const
    {
        return std::find(m_Panels.begin(), m_Panels.end(), panel) != m_Panels.end();
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
        // Panel 矩形使用 Dock 内局部坐标，避免 Tab 栏和内容区的坐标约定分裂。
        Panel *active = GetActivePanel();
        if (active)
            active->SetLayoutRect(m_X + m_PanelX, m_Y + m_PanelY,
                                  m_PanelW, m_PanelH);
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
            canvas.FillRect(tabX, tabY, tabW, m_MenuBarHeight,
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

    // ════════════════════════════════════════════════════════════
    // 分屏 / 切割 / 合并 —— Dock 的"重新划分自己"职责
    // 思路（沿用砚台原设计）：一次切割 = 造一条共享分割线，新旧两侧各占一侧。
    // ════════════════════════════════════════════════════════════

    Dock *Dock::Split(Direction dir, float size)
    {
        if (!m_Layout || !m_Splittable)
            return nullptr;
        if (size <= 0.0f || size >= 1.0f)
            return nullptr;

        // 确定新边界朝向与比例
        BoundaryOrientation orientation =
            (dir == Direction::Left || dir == Direction::Right)
                ? BoundaryOrientation::Vertical
                : BoundaryOrientation::Horizontal;

        // 造一条新分割线（集中注册表管理；由本 Dock 与邻居共享）
        BoundaryId newBoundary = m_Layout->AddBoundary(orientation, size);
        if (newBoundary == InvalidBoundary)
            return nullptr;

        // 新 Dock：接管当前激活面板，其余面板留下
        Dock *newDock = new Dock();
        newDock->SetDockLayout(m_Layout);
        newDock->SetSplittable(m_Splittable);
        newDock->SetHostRepaint(m_HostRepaint);

        // 让活跃面板跟随新 Dock（分屏时把当前内容切到新一侧）
        Panel *active = GetActivePanel();
        if (active)
        {
            RemovePanelInternal(active);
            newDock->AddPanel(active, /*title留空*/ "");
        }

        // 共享边分配：新 Dock 那侧 + 本 Dock 相对侧
        switch (dir)
        {
        case Direction::Left:
            newDock->GetBoundarySlots().right = newBoundary;
            m_Boundary.left = newBoundary;
            break;
        case Direction::Right:
            newDock->GetBoundarySlots().left = newBoundary;
            m_Boundary.right = newBoundary;
            break;
        case Direction::Top:
            newDock->GetBoundarySlots().bottom = newBoundary;
            m_Boundary.top = newBoundary;
            break;
        case Direction::Bottom:
            newDock->GetBoundarySlots().top = newBoundary;
            m_Boundary.bottom = newBoundary;
            break;
        }

        // 新旧均记下"我是从这条边切出来的"（供被动合并找邻居）
        newDock->SetMergedBoundaryId(newBoundary);
        SetMergedBoundaryId(newBoundary);
        // 同宗：新 Dock 记我是它父亲
        newDock->SetDockFather(this);

        // 入布局并重排
        m_Layout->AddDock(newDock);
        m_Layout->RecalcLayout();
        RequestRepaint();
        return newDock;
    }

    void Dock::Merge()
    {
        // 被动合并：本 Dock 已空，沿 mergedBoundary 并回对侧邻居并删掉自己。
        // 由外层（DockLayout 移除临时 dock 前）调用。
        if (!m_Layout || m_MergedBoundaryId == InvalidBoundary)
            return;

        // 找一个共享 mergedBoundary 的邻居
        for (Dock *dock : m_Layout->GetDockList())
        {
            if (!dock || dock == this)
                continue;
            const auto &slots = dock->GetBoundarySlots();
            if (slots.top == m_MergedBoundaryId ||
                slots.bottom == m_MergedBoundaryId ||
                slots.left == m_MergedBoundaryId ||
                slots.right == m_MergedBoundaryId)
            {
                // 拆掉缝合线（邻居侧槽恢复为贴外框/留给布局重排）
                m_Layout->RemoveBoundary(m_MergedBoundaryId);
                // 从布局移除并让布局重排；随后由布局释放本 Dock
                m_Layout->RemoveDock(this);
                RequestRepaint();
                return;
            }
        }

        // 没找到邻居，仍直接拆线移除自己
        m_Layout->RemoveBoundary(m_MergedBoundaryId);
        m_Layout->RemoveDock(this);
    }

    // 内部：不移面板版本（Split 用，用于把活跃面板移到新 Dock）
    void Dock::RemovePanelInternal(Panel *panel)
    {
        auto it = std::find(m_Panels.begin(), m_Panels.end(), panel);
        if (it == m_Panels.end())
            return;
        const int idx = (int)(it - m_Panels.begin());
        m_Panels.erase(it);
        m_Titles.erase(m_Titles.begin() + idx);
        if (m_ActiveIndex >= (int)m_Panels.size())
            m_ActiveIndex = (int)m_Panels.size() - 1;
        if (m_Panels.empty())
            m_ActiveIndex = -1;
    }

} // namespace X_Y
