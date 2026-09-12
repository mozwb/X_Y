#include "UI/dock/tabdock.h"
#include "UI/DockLayout/DockLayout.h"

#include <cmath>

namespace X_Y
{
    TabDock::TabDock()
    {
        SetMenuBarHeight(30);
    }

    void TabDock::SetDetachToWindowHandler(
        std::function<bool(Panel *, const std::string &, int, int)> handler)
    {
        m_DetachToWindow = std::move(handler);
    }

    void TabDock::RouteInput(UIInputEvent &e)
    {
        auto *mouse = dynamic_cast<UIMouseEvent *>(&e);
        if (!mouse)
        {
            Dock::RouteInput(e);
            return;
        }

        const bool inTabBar = mouse->y >= 0 && mouse->y < GetMenuBarHeight() &&
                              mouse->x >= 0 && mouse->x < TabBarTotalWidth();

        if (mouse->action == MouseAction::Press && inTabBar)
        {
            m_DragPanel = nullptr;
            m_DraggingPanel = false;
            m_DragStartX = mouse->x;
            m_DragStartY = mouse->y;

            // tab 栏几何与绘制同源（TabIndexAt 用同一个 kTabWidth）
            const int tabIndex = TabIndexAt(mouse->x);
            if (tabIndex >= 0 && tabIndex < GetPanelCount())
            {
                ActivatePanel(tabIndex);
                // 记住可能是"拖出"的起点：超过阈值才算拖动，单纯点击只是切 tab
                m_DragPanel = GetPanel(tabIndex);
            }
            e.Handled = true; // tab 栏事件归 tab 栏，不穿透到 Panel
            return;
        }

        // 拖动中：移动累积到阈值就进入拖拽态；松手时结算落点
        if (m_DragPanel)
        {
            if (mouse->action == MouseAction::Move && !m_DraggingPanel)
            {
                m_DraggingPanel = std::abs(mouse->x - m_DragStartX) >= kDragThreshold ||
                                  std::abs(mouse->y - m_DragStartY) >= kDragThreshold;
            }

            if (m_DraggingPanel)
            {
                if (mouse->action == MouseAction::Release)
                {
                    DropPanel(mouse->x, mouse->y);
                    ResetPanelDrag();
                }
                e.Handled = true;
                return;
            }

            if (mouse->action == MouseAction::Release)
                ResetPanelDrag();
        }

        // 其余情况（含 tab 栏内的 Move/Release）：交给基类，
        // 基类会把 tab 栏条带整体吞掉，不会穿透到 Panel。
        Dock::RouteInput(e);
    }

    void TabDock::DropPanel(int localX, int localY)
    {
        DockLayout *layout = GetDockLayout();
        if (!layout || !m_DragPanel)
            return;

        // 落点换算成布局绝对坐标（Dock 用绝对坐标，Panel 才用局部坐标）
        const int x = GetX() + localX;
        const int y = GetY() + localY;

        // 落点命中哪个 Dock —— 复用 DockLayout::HitTestDock，
        // 与绘制 z 序 / 事件命中保持一致（免得三处各写一套顺序）。
        Dock *target = layout->HitTestDock(x, y);
        if (target == this)
            target = nullptr; // 落回自己：不算跨 Dock 移动

        if (!target || !target->CanAddPanel())
        {
            const bool insideThisDock =
                x >= GetX() && x < GetX() + GetWidth() &&
                y >= GetY() && y < GetY() + GetHeight();
            if (!target && !insideThisDock && m_DetachToWindow)
            {
                std::string title;
                Panel *panel = DetachPanel(m_DragPanel, &title);
                if (panel && !m_DetachToWindow(panel, title, x, y))
                    AddPanel(panel, title); // 摘出失败：放回自己，别丢面板
            }
            return;
        }

        std::string title;
        Panel *panel = DetachPanel(m_DragPanel, &title);
        if (panel)
            target->AddPanel(panel, title);
    }

    void TabDock::ResetPanelDrag()
    {
        m_DragPanel = nullptr;
        m_DraggingPanel = false;
    }
} // namespace X_Y
