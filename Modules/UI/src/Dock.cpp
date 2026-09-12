#include "../dock/Dock.h"
#include "DockLayout/DockLayout.h"
#include "../UiCore/UINode.h"
#include <algorithm>

namespace X_Y
{

    // ── UINodeView：Dock 在 DockLayout 坐标系（= 布局绝对）里的矩形 ──
    // self    = Dock 的完整矩形
    // content = 面板区（避开 tab 栏）—— 用【生效面板区 m_EffPanel*】，
    //           因为那才是真正交给 Panel::SetLayoutRect 的矩形。
    //           若这里改用 m_MenuBarHeight 推算，就等于又引入了第二份"面板区真相"，
    //           正是过去 m_Panel* / m_EffPanel* / Panel::GetX() 三份数据打架的根源。
    UINodeView View(const Dock &node)
    {
        UINodeView v;
        v.self = Rect{ node.GetX(), node.GetY(), node.GetWidth(), node.GetHeight() };
        v.content = Rect{ node.GetEffPanelX(), node.GetEffPanelY(),
                          node.GetEffPanelW(), node.GetEffPanelH() };
        v.visible = true; // Dock 不隐藏自身（空 dock 也占位）
        return v;
    }

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
        UpdatePanelRects(); // 重排后同步所有 Panel 的布局矩形
    }

    void Dock::RecalcRect()
    {
        if (!m_Layout)
            return;

        if (m_Boundary.top == InvalidBoundary &&
            m_Boundary.bottom == InvalidBoundary &&
            m_Boundary.left == InvalidBoundary &&
            m_Boundary.right == InvalidBoundary)
        {
            m_X = 0;
            m_Y = 0;
            m_W = std::max(0, m_LayoutW);
            m_H = std::max(0, m_LayoutH);
            return;
        }

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
        else
        {
            m_X = left;
            m_Y = top;
            m_W = 0;
            m_H = 0;
        }
    }

    bool Dock::IsBoundaryPositionValid(BoundaryId id, float line) const
    {
        if (!m_Layout)
            return true;

        auto boundaryLine = [this](BoundaryId boundaryId) -> const float *
        {
            const Boundary *boundary = m_Layout->GetBoundary(boundaryId);
            return boundary ? &boundary->line : nullptr;
        };

        if (id == m_Boundary.left)
        {
            const float *right = boundaryLine(m_Boundary.right);
            return line + m_MinWidth <= (right ? *right : 1.0f);
        }
        if (id == m_Boundary.right)
        {
            const float *left = boundaryLine(m_Boundary.left);
            return (left ? *left : 0.0f) + m_MinWidth <= line;
        }
        if (id == m_Boundary.top)
        {
            const float *bottom = boundaryLine(m_Boundary.bottom);
            return line + m_MinHeight <= (bottom ? *bottom : 1.0f);
        }
        if (id == m_Boundary.bottom)
        {
            const float *top = boundaryLine(m_Boundary.top);
            return (top ? *top : 0.0f) + m_MinHeight <= line;
        }
        return true;
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

    // ── 边界命中（供拖分割线）──
    // ★ 坐标约定：入参 x/y 是【布局绝对坐标】，不是 Dock 局部坐标！
    //   调用方 DockLayout::RouteInput 传的是未平移的 e.x/e.y，本函数内部
    //   也用 m_Layout->GetBoundaryPosition()（绝对像素）比较。
    //   ⚠️ 这与 HitTestPanel（Dock 局部）【不同】—— 别顺手"统一"成局部，
    //      否则拖分割线会命中不到（boundary 是全布局共享的，天然属于绝对层）。
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
            float start = 0.0f;
            float end = 1.0f;
            if (!m_Layout->GetBoundarySize(id, start, end))
                return false;
            const int extent = vertical ? m_Layout->GetLayoutHeight()
                                        : m_Layout->GetLayoutWidth();
            const int spanLo = static_cast<int>(start * extent);
            const int spanHi = static_cast<int>(end * extent);
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

    // ── 命中面板区（入参 x/y 为【Dock 局部坐标】）──
    // 与绘制共用 UINodeView 的内容区描述（= m_EffPanel*）。
    Panel *Dock::HitTestPanel(int x, int y) const
    {
        if (!View(*this).Content().Contains(x, y))
            return nullptr;
        return GetActivePanel();
    }

    // ── 输入：命中 tab 栏切 tab；否则下传给激活 Panel ──
    void Dock::RouteInput(UIInputEvent &e)
    {
        // e.x/e.y 为 dock 内坐标（0,0 = dock 左上，由 DockLayout 平移好）
        if (auto *me = dynamic_cast<UIMouseEvent *>(&e))
        {
            if (me->action == MouseAction::Press)
                m_MouseCapturePanel = nullptr;

            // ★ tab 栏是 UI chrome：落在这一条带内的鼠标事件【一律吞掉】，
            //   绝不穿透到下面的 Panel（否则点 tab 会把点击也送给内容）。
            const bool inTabBar = (e.y >= 0 && e.y < m_MenuBarHeight &&
                                   e.x >= 0 && e.x < TabBarTotalWidth());
            if (inTabBar)
            {
                if (me->action == MouseAction::Press)
                {
                    const int idx = TabIndexAt(e.x);
                    if (idx >= 0 && idx != m_ActiveIndex)
                        ActivatePanel(idx);
                }
                e.Handled = true; // 无论命中哪个 tab，这一条带都归 tab 栏
                return;
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

        Panel *p = m_MouseCapturePanel;
        if (!p)
            p = HitTestPanel(e.x, e.y);

        if (p)
        {
            if (auto *me = dynamic_cast<UIMouseEvent *>(&e);
                me && me->action == MouseAction::Press)
                m_MouseCapturePanel = p;

            // 下钻到 Panel：e 现在是【Dock 局部坐标】，而 View(Panel).self 正是
            // Panel 在 Dock 坐标系里的位置 —— 直接 ToLocal 即可（不再有
            // `e.x -= p->GetX() - m_X` 那种"绝对减绝对"的补丁）。
            const UINodeView pv = View(*p);
            ToLocal(pv, e.x, e.y);
            p->OnInput(e);
            ToParent(pv, e.x, e.y); // 还原成 Dock 局部，供上层保持视角
        }

        if (auto *me = dynamic_cast<UIMouseEvent *>(&e);
            me && me->action == MouseAction::Release)
            m_MouseCapturePanel = nullptr;
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
        UpdatePanelRects();
        RequestRepaint();
    }

    void Dock::UpdatePanelRects()
    {
        Panel *active = GetActivePanel();
        if (!active)
            return;

        // 计算实际生效的面板区（钳到 Dock 内）。
        // ⚠️ 注意：钳制结果【不写回】m_PanelW/m_PanelH —— 那对是"声明值"，
        //    必须跨 resize 存活（否则窗口缩到 0 再放大，声明尺寸就被永久压掉了）。
        //    改为把生效矩形缓存到 m_EffPanel*，命中判定与交给 Panel 的矩形都用它，
        //    保证"画在哪"和"点在哪"仍然是同一份数据。
        m_EffPanelX = std::clamp(m_PanelX, 0, std::max(0, m_W));
        m_EffPanelY = std::clamp(m_PanelY, 0, std::max(0, m_H));
        m_EffPanelW = std::clamp(m_PanelW, 0, std::max(0, m_W - m_EffPanelX));
        m_EffPanelH = std::clamp(m_PanelH, 0, std::max(0, m_H - m_EffPanelY));

        // ★ Panel 的矩形是【Dock 局部坐标】（父坐标系 = Dock）。
        //   不是布局绝对坐标 —— 绝对坐标只活在 DockLayout/Dock 这一侧，
        //   到 Dock↔Panel 交界就转成相对，Panel 及以下全程相对。
        active->SetLayoutRect(m_EffPanelX, m_EffPanelY, m_EffPanelW, m_EffPanelH);
    }

    // ── 绘制：Panel 内容 + tab 栏 ──
    // 坐标约定：本函数收到的是【Dock 局部坐标】canvas —— DockLayout 已压过
    //           PushOrigin(dock->GetX(), dock->GetY())。
    //
    // ★ 绘制顺序：先 Panel 内容、后 tab 栏。tab 栏【最后画 = 压在最上层】，
    //   这样任何内容（含滚动溢出）都盖不住它。
    // ★ 几何来源：面板区走 View(*this).Content()（= m_EffPanel*），
    //   与 HitTestPanel / RouteInput 的输入偏移同源，不再各算一份。
    void Dock::OnPaint(Canvas &canvas)
    {
        const UINodeView view = View(*this);

        canvas.FillRect(0, 0, m_W, m_H, 0xFF232527);

        // ① 激活 Panel 内容：裁到面板区，绝不允许画到 tab 栏/边界上
        Panel *active = GetActivePanel();
        if (active)
        {
            const Rect &area = view.content;
            canvas.PushOrigin(area.x, area.y);
            canvas.SetClip(0, 0, area.w, area.h);
            active->OnPaint(canvas);
            canvas.ResetClip();
            canvas.PopOrigin();
        }

        // ② tab 栏（最后画，最上层；几何走 TabBarTotalWidth/TabIndexAt，与命中同源）
        for (int i = 0; i < (int)m_Panels.size(); ++i)
        {
            const bool isActive = (i == m_ActiveIndex);
            canvas.FillRect(i * kTabWidth, 0, kTabWidth, m_MenuBarHeight,
                            isActive ? 0xFF007ACC : 0xFF3E3E42);
            // (字体绘制后续接 FontLibrary；先只画色块+索引)
        }
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
