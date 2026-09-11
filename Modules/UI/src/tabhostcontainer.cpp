#include "UI/Container/tabhostcontainer.h"
#include "UI/DockLayout/DockLayout.h"

namespace X_Y
{
    void TabHostContainer::SetDockLayout(DockLayout *layout)
    {
        Container::SetDockLayout(layout);
        ConfigureTabDocks();
    }

    void TabHostContainer::ConfigureTabDocks()
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

    void TabHostContainer::ConfigureTabDock(TabDock &dock)
    {
        dock.SetDetachToWindowHandler(
            [this](Panel *panel, const std::string &title, int x, int y)
            { return HandlePanelDrop(panel, title, x, y); });
    }

    bool TabHostContainer::HandlePanelDrop(Panel *panel,
                                           const std::string &title,
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
                return true;
        }

        auto *window = new TabContainer();
        window->SetWindowTitle(title);
        window->setSize(700, 500);
        if (!window->AddSinglePanel(panel, title))
        {
            delete window;
            return false;
        }

        if (!window->show())
        {
            delete window;
            return false;
        }

        window->MoveAndResizePhysical(screenX - 120, screenY - 20, 700, 500);
        return true;
    }
} // namespace X_Y
