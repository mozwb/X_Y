#pragma once

#include "Movement/include/movements.h"
#include "UI/include/dock/DockPanel.h"
#include "Widget/include/XWidget.h"

namespace X_Y {

// ============================================================
// DockLayer — 监听全局窗口拖拽事件
// 检测 XWidget 拖入 Docker 窗口区域，执行停靠
// ============================================================

class Docker;

class DockLayer : public Layer {
public:
    DockLayer(Docker* owner);
    ~DockLayer() override = default;

    void OnEvent(XMovement* event) override;

private:
    Docker* m_Owner = nullptr;

    // 拖拽跟踪
    XWidget* m_DraggingWidget = nullptr;
    bool     m_IsDragging = false;
};

} // namespace X_Y
