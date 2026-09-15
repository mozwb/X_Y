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
            [this](std::unique_ptr<Panel> panel, const std::string &title, int x, int y)
            { return HandlePanelDrop(std::move(panel), title, x, y); });
    }

    bool TabHostContainer::HandlePanelDrop(std::unique_ptr<Panel> panel,
                                           const std::string &title,
                                           int, int)
    {
        if (!panel)
            return false;

        int screenX = 0;
        int screenY = 0;
        BaseWin::GetMouseScreenPos(screenX, screenY);

        // ① 优先：交给落点窗口收养
        auto *target = dynamic_cast<Container *>(
            BaseWin::GetWindowAt(screenX, screenY));
        if (target && target != this)
        {
            int targetX = screenX;
            int targetY = screenY;
            target->ScreenToClient(targetX, targetY);
            DockLayout *targetLayout = target->GetDockLayout();
            if (targetLayout)
            {
                // ⚠️ AddPanelAt 按值收 unique_ptr：失败时 panel 已被释放。
                //    所以先用裸指针记住"它还在不在"，失败就直接返回，
                //    不能再拿它去建窗口。
                if (targetLayout->AddPanelAt(std::move(panel), targetX, targetY, title))
                    return true;
                return false; // panel 已随参数析构，无法再回退建窗
            }
        }

        // ② 回退：新建一个独立窗口收容。
        //    走到这里说明还没人接管，panel 仍在本函数手里。
        auto *window = new TabContainer();
        window->SetWindowTitle(title);
        window->setSize(700, 500);

        // AddSinglePanel 接管 panel；失败则 panel 被释放，window 也要销毁。
        if (!window->AddSinglePanel(panel.release(), title))
        {
            delete window;
            return false;
        }

        if (!window->show())
        {
            delete window; // 窗口销毁 → 其 Dock 释放已接管的 panel
            return true;   // panel 已被安置（随窗口销毁），不算"没接管"
        }

        window->MoveAndResizePhysical(screenX - 120, screenY - 20, 700, 500);
        return true;
    }
} // namespace X_Y
