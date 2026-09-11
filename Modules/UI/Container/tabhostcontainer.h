#pragma once

#include "UI/Container/Container.h"
#include "tabcontainer.h"

namespace X_Y
{
    class TabHostContainer : public Container
    {
    public:
        using Container::Container;

        void SetDockLayout(DockLayout *layout) override;

    private:
        void ConfigureTabDocks();
        void ConfigureTabDock(TabDock &dock);
        bool HandlePanelDrop(Panel *panel, const std::string &title,
                             int x, int y);
    };
} // namespace X_Y