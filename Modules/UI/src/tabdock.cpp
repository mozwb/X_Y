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

        if (mouse->action == MouseAction::Press &&
            mouse->button == Input_t::Mouse::ButtonLeft)
        {
            m_DragPanel = nullptr;
            m_DraggingPanel = false;
            m_DragStartX = mouse->x;
            m_DragStartY = mouse->y;

            const int tabIndex = mouse->y >= 0 && mouse->y < GetMenuBarHeight()
                                     ? mouse->x / kTabWidth
                                     : -1;
            if (tabIndex >= 0 && tabIndex < GetPanelCount())
            {
                ActivatePanel(tabIndex);
                m_DragPanel = GetPanel(tabIndex);
                e.Handled = true;
                return;
            }
        }

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
                    DropPanel(mouse->x, mouse->y);
                e.Handled = true;
                if (mouse->action == MouseAction::Release)
                    ResetPanelDrag();
                return;
            }

            if (mouse->action == MouseAction::Release)
                ResetPanelDrag();
        }

        Dock::RouteInput(e);
    }

    void TabDock::DropPanel(int localX, int localY)
    {
        DockLayout *layout = GetDockLayout();
        if (!layout || !m_DragPanel)
            return;

        const int x = GetX() + localX;
        const int y = GetY() + localY;
        Dock *target = nullptr;
        for (Dock *dock : layout->GetDockList())
        {
            if (!dock || dock == this ||
                x < dock->GetX() || x >= dock->GetX() + dock->GetWidth() ||
                y < dock->GetY() || y >= dock->GetY() + dock->GetHeight())
                continue;
            target = dock;
            break;
        }

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
                    AddPanel(panel, title);
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
