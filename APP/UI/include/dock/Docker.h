#pragma once

#include "UI/include/Container/Container.h"
#include "UI/include/dock/DockPanel.h"
#include "XCore/include/XYCore.h"
#include <string>

namespace X_Y {

class DockLayer;

// ============================================================
// Docker — Dock 停靠系统主窗口
// 管理五区域划分，挂载 DockLayer 接收拖拽事件
// ============================================================

class Docker : public Container {
public:
    enum Area { Top, Left, Center, Right, Bottom };

    Docker(const std::string& title, int w, int h, XWidget* parent = nullptr);
    ~Docker() override;

    void Dock(XWidget* panel, const std::string& title, Area area);
    void RecalcLayout();
    Area HitTestArea(int clientX, int clientY) const;

    // 供 DockLayer 调用的停靠指示器（后续扩展）
    // void ShowDropIndicator(Area area);

private:
    Scope<DockPanel> m_Panels[5];
    DockLayer*       m_Layer = nullptr;

    DockPanel* Panel(Area a) const { return m_Panels[(int)a].get(); }
};

} // namespace X_Y
