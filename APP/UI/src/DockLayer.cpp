#include "dock/DockLayer.h"
#include "dock/Docker.h"
#include "Movement/include/AppMovement.h"
#include "Movement/include/MouseMovement.h"

namespace X_Y {

DockLayer::DockLayer(Docker* owner)
    : m_Owner(owner)
{
}

void DockLayer::OnEvent(XMovement* event)
{
    if (!event || !m_Owner) return;

    if (event->sender == static_cast<MovementSender>(m_Owner))
        return;

    auto* senderWidget = static_cast<XWidget*>(event->sender);
    if (!senderWidget || !senderWidget->GetNativeHandle())
        return;

    auto* mov = dynamic_cast<Movement*>(event);
    if (!mov) return;

    switch (mov->GetType())
    {
    case MovementType::WindowDragBegin:
    {
        m_DraggingWidget = senderWidget;
        m_IsDragging = true;
        event->Handled = true;
        m_Owner->SetDragPreviewMode(true);
        m_Owner->StartDragMonitor();
        XINFO("[DockLayer] drag BEGIN: {}", senderWidget->getname());
        break;
    }

    case MovementType::WindowDragEnd:
    {
        if (!m_IsDragging || !m_DraggingWidget)
        {
            m_IsDragging = false;
            m_DraggingWidget = nullptr;
            break;
        }

        m_Owner->StopDragMonitor();
        m_Owner->SetDragPreviewMode(false);

        int mx, my;
        BaseWin::GetMouseScreenPos(mx, my);
        BaseWin* winUnder = BaseWin::GetWindowAt(mx, my);

        if (winUnder == m_Owner) {
            m_Owner->ScreenToClient(mx, my);
            Docker::Area area = m_Owner->HitTestArea(mx, my);
            m_Owner->Dock(m_DraggingWidget, m_DraggingWidget->getname(), area);
            XINFO("[DockLayer] dock done: {} -> {}", m_DraggingWidget->getname(), (int)area);
        }

        m_Owner->HideDropPreviews();
        event->Handled = true;
        m_IsDragging = false;
        m_DraggingWidget = nullptr;
        break;
    }

    case MovementType::MouseMoved:
    {
        // 系统拖拽模态循环中 MouseMoved 不会到达这里
        // 预览由 Docker 自己的定时器更新
        break;
    }

    default:
        break;
    }
}

}
// namespace X_Y
