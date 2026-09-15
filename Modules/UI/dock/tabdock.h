#pragma once

#include "UI/dock/Dock.h"

#include <functional>
#include <memory>

namespace X_Y
{
    class TabDock : public Dock
    {
    public:
        TabDock();

        // 拖出到窗口时的回调（由壳注入）。
        // ★ 参数是 unique_ptr：把 Panel 的所有权交给回调。
        //   返回 true = 回调已接管（所有权归它）；返回 false = 未能接管，
        //   由 DropPanel 负责放回原处（不会泄漏）。
        using DetachToWindowHandler = std::function<bool(
            std::unique_ptr<Panel>, const std::string &, int, int)>;

        void SetDetachToWindowHandler(DetachToWindowHandler handler);
        void RouteInput(UIInputEvent &e) override;

    private:
        // ⚠️ tab 宽度用基类 Dock::kTabWidth（绘制与命中必须同源），
        //    这里不再重复定义，避免两处各写一份导致对不上。
        static constexpr int kDragThreshold = 4;

        void DropPanel(int localX, int localY);
        void ResetPanelDrag();

        Panel *m_DragPanel = nullptr; // 借用：正在拖的那个（不拥有）
        bool m_DraggingPanel = false;
        int m_DragStartX = 0;
        int m_DragStartY = 0;
        DetachToWindowHandler m_DetachToWindow;
    };
} // namespace X_Y
