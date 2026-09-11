#pragma once

#include "UI/Container/Container.h"
#include "../dock/tabdock.h"

#include <string>

namespace X_Y
{
    class TabContainer : public Container
    {
    public:
        using Container::Container;

        void SetWindowTitle(const std::string &title);
        void SetDockLayout(DockLayout *layout) override;

    protected:
        Dock *CreateSinglePanelDock() override;

    private:
        void ConfigureTabDocks();
        void ConfigureTabDock(TabDock &dock);
        bool HandlePanelDrop(Panel *panel, const std::string &title,
                             int x, int y);

        std::string m_WindowTitle;
    };
} // namespace X_Y
