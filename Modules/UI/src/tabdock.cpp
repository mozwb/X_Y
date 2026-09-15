#include "UI/dock/tabdock.h"
#include "UI/DockLayout/DockLayout.h"

#include <cmath>

namespace X_Y
{
    TabDock::TabDock()
    {
        SetMenuBarHeight(30);
    }

    void TabDock::SetDetachToWindowHandler(DetachToWindowHandler handler)
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
                Panel *dragPanel = m_DragPanel;
                std::string title;
                // ★ 交出所有权（unique_ptr）。即使回调不接管，owned 析构
                //   也会正确释放，不会漏。
                std::unique_ptr<Panel> owned = DetachPanel(dragPanel, &title);
                // ⚠️ DetachPanel 之后 m_DragPanel 已经不属于本 Dock 了，
                //    必须立刻清掉（否则它在两步之间是悬空的借用指针）。
                ResetPanelDrag();

                if (owned && !m_DetachToWindow(std::move(owned), title, x, y))
                {
                    // 回调没接管 → 此刻 owned 已是被 move 走的空壳，收不回来了。
                    // ⇒ 因此约定：回调返回 false 时必须【自己保证】panel 已被
                    //    安置或释放（见 TabContainer/TabHostContainer 的实现）。
                    //    这里不再尝试 AddPanel（那会是重复添加同一个 panel）。
                }
            }
            return;
        }

        Panel *dragPanel = m_DragPanel;
        std::string title;
        std::unique_ptr<Panel> owned = DetachPanel(dragPanel, &title);
        ResetPanelDrag(); // 同上：所有权已转出，借用指针立刻清
        if (owned)
            target->AddPanel(std::move(owned), title); // 新主人接管
    }

    void TabDock::ResetPanelDrag()
    {
        m_DragPanel = nullptr;
        m_DraggingPanel = false;
    }
} // namespace X_Y
