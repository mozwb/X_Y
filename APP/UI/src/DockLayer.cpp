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

    // 忽略 Docker 自己发出的事件
    if (event->sender == static_cast<MovementSender>(m_Owner))
        return;

    // 忽略没有有效窗口的 sender
    auto* senderWidget = static_cast<XWidget*>(event->sender);
    if (!senderWidget || !senderWidget->GetNativeWindow())
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
        XINFO("[DockLayer] 拖拽开始: {}", senderWidget->getname());
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

        int mx, my;
        BaseWin::GetMouseScreenPos(mx, my);
        BaseWin* winUnder = BaseWin::GetWindowAt(mx, my);

        if (winUnder == m_Owner) {
            m_Owner->ScreenToClient(mx, my);
            Docker::Area area = m_Owner->HitTestArea(mx, my);
            m_Owner->Dock(m_DraggingWidget, m_DraggingWidget->getname(), area);
            XINFO("[DockLayer] 停靠完成: {} -> {}", m_DraggingWidget->getname(), (int)area);
        }

        event->Handled = true;
        m_IsDragging = false;
        m_DraggingWidget = nullptr;
        break;
    }

    case MovementType::MouseMoved:
    {
        if (!m_IsDragging) break;

        int mx, my;
        BaseWin::GetMouseScreenPos(mx, my);
        BaseWin* winUnder = BaseWin::GetWindowAt(mx, my);

        if (winUnder == m_Owner) {
            m_Owner->ScreenToClient(mx, my);
            Docker::Area area = m_Owner->HitTestArea(mx, my);
            // TODO: 显示停靠指示器
        }
        event->Handled = true;
        break;
    }

    default:
        break;
    }
}

}
// namespace X_Y
