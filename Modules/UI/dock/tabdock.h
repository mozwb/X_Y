#pragma once

#include "UI/dock/Dock.h"

#include <functional>

namespace X_Y
{
    class TabDock : public Dock
    {
    public:
        TabDock();

        void SetDetachToWindowHandler(
            std::function<bool(Panel *, const std::string &, int, int)> handler);
        void RouteInput(UIInputEvent &e) override;

    private:
        // ⚠️ tab 宽度用基类 Dock::kTabWidth（绘制与命中必须同源），
        //    这里不再重复定义，避免两处各写一份导致对不上。
        static constexpr int kDragThreshold = 4;

        void DropPanel(int localX, int localY);
        void ResetPanelDrag();

        Panel *m_DragPanel = nullptr;
        bool m_DraggingPanel = false;
        int m_DragStartX = 0;
        int m_DragStartY = 0;
        std::function<bool(Panel *, const std::string &, int, int)> m_DetachToWindow;
    };
} // namespace X_Y
