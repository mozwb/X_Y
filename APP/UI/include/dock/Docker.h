#pragma once

#include "UI/include/Container/Container.h"
#include "UI/include/dock/DockPanel.h"
#include "UI/include/Component/Overlay.h"
#include "XCore/include/XYCore.h"
#include <string>
#include <thread>
#include <atomic>

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

    // 停靠预览指示器（五个区域同时显示）
    void ShowDropPreviews();
    void HideDropPreviews();

    // 拖拽检测线程（系统拖拽模态循环中轮询鼠标位置）
    void StartDragMonitor();
    void StopDragMonitor();

    // 由 DockLayer 设置拖拽状态
    void SetDragPreviewMode(bool on) { m_IsDragPreview = on; }

private:
    Scope<DockPanel> m_Panels[5];
    DockLayer*       m_Layer = nullptr;
    Scope<Overlay>   m_Previews[5];

    std::thread m_DragThread;
    std::atomic<bool> m_DragMonitoring{ false };
    bool m_IsDragPreview = false;

    DockPanel* Panel(Area a) const { return m_Panels[(int)a].get(); }
};

} 
// namespace X_Y
