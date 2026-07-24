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

    // 不处理 Docker 自己发的事件
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

        m_Owner->SetDragPreviewMode(false);

        // 获取鼠标屏幕坐标

        int mx, my;
        BaseWin::GetMouseScreenPos(mx, my);

        
        // 检查鼠标是否在 Docker 窗口上
        
        {
            int clientX = mx, clientY = my;
            m_Owner->ScreenToClient(clientX, clientY);
            int w = m_Owner->GetActualWidth();
            int h = m_Owner->GetActualHeight();

            if (clientX >= 0 && clientX < w && clientY >= 0 && clientY < h) {
                Docker::Area area = m_Owner->HitTestArea(clientX, clientY);
                m_Owner->Dock(m_DraggingWidget, m_DraggingWidget->getname(), area);
                XINFO("[DockLayer] dock done: {} -> area {}", m_DraggingWidget->getname(), (int)area);
            }
        }

        m_Owner->HideDropPreviews();
        event->Handled = true;
        m_IsDragging = false;
        m_DraggingWidget = nullptr;
        break;
    }

    case MovementType::MouseMoved:
    {
        // 仅在拖拽模式下更新预览

        if (!m_IsDragging || !m_Owner->IsDragPreviewMode())
            break;

        // 获取鼠标屏幕坐标并转为 Docker 客户区坐标
        
        auto& mm = dynamic_cast<const MouseMoved&>(*mov);
        float mx = mm.GetX();
        float my = mm.GetY();

        // MouseMoved 的坐标是相对于 sender 窗口客户区的
        // 想要判断是否在 Docker 上，需要用 Docker 的坐标系

        int screenX, screenY;
        BaseWin::GetMouseScreenPos(screenX, screenY);

        int clientX = screenX, clientY = screenY;
        m_Owner->ScreenToClient(clientX, clientY);
        int w = m_Owner->GetActualWidth();
        int h = m_Owner->GetActualHeight();

        if (clientX >= 0 && clientX < w && clientY >= 0 && clientY < h) {
            m_Owner->ShowDropPreviews();
        } else {
            m_Owner->HideDropPreviews();
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
