#pragma once

#include "UI/Container/Container.h"
#include "../dock/tabdock.h"

#include <memory>
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
        // 拖出到窗口的回调（见 TabDock::DetachToWindowHandler）。
        // ★ 收 unique_ptr：本函数负责接管或释放；返回 true 表示已接管。
        bool HandlePanelDrop(std::unique_ptr<Panel> panel, const std::string &title,
                             int x, int y);

        std::string m_WindowTitle;
    };
} // namespace X_Y
