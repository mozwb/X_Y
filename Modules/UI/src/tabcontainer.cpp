#include "UI/Container/tabcontainer.h"
#include "UI/DockLayout/DockLayout.h"

namespace X_Y
{
    void TabContainer::SetWindowTitle(const std::string &title)
    {
        m_WindowTitle = title.empty() ? "Detached Panel" : title;
        setTitle(m_WindowTitle.c_str());
    }

    void TabContainer::SetDockLayout(DockLayout *layout)
    {
        Container::SetDockLayout(layout);
        ConfigureTabDocks();
    }

    Dock *TabContainer::CreateSinglePanelDock()
    {
        auto *dock = new TabDock();
        ConfigureTabDock(*dock);
        return dock;
    }

    void TabContainer::ConfigureTabDocks()
    {
        DockLayout *layout = GetDockLayout();
        if (!layout)
            return;

        for (Dock *dock : layout->GetDockList())
        {
            if (auto *tabDock = dynamic_cast<TabDock *>(dock))
                ConfigureTabDock(*tabDock);
        }
    }

    void TabContainer::ConfigureTabDock(TabDock &dock)
    {
        dock.SetDetachToWindowHandler(
            [this](Panel *panel, const std::string &title, int x, int y)
            { return HandlePanelDrop(panel, title, x, y); });
    }

    bool TabContainer::HandlePanelDrop(Panel *panel, const std::string &title,
                                       int, int)
    {
        if (!panel)
            return false;

        int screenX = 0;
        int screenY = 0;
        BaseWin::GetMouseScreenPos(screenX, screenY);

        auto *target = dynamic_cast<Container *>(
            BaseWin::GetWindowAt(screenX, screenY));
        if (target && target != this)
        {
            int targetX = screenX;
            int targetY = screenY;
            target->ScreenToClient(targetX, targetY);
            DockLayout *targetLayout = target->GetDockLayout();
            if (targetLayout &&
                targetLayout->AddPanelAt(panel, targetX, targetY, title))
            {
                destroy();
                return true;
            }
        }

        return false;
    }
} // namespace X_Y
