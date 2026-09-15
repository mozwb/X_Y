#pragma once

#include "UI/Container/Container.h"
#include "tabcontainer.h"

#include <memory>

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
        // 拖出到窗口的回调（见 TabDock::DetachToWindowHandler）。
        // ★ 收 unique_ptr：本函数负责接管或释放；返回 true 表示已接管。
        bool HandlePanelDrop(std::unique_ptr<Panel> panel, const std::string &title,
                             int x, int y);
    };
} // namespace X_Y
