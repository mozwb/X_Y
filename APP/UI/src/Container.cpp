#include "Container/Container.h"
#include "Component/Component.h"
#include "Movement/include/KeyMovement.h"
#include "Movement/include/MouseMovement.h"
#include "Widget/include/Dpi.h"

namespace X_Y {

Container::Container(XWidget* parent)
    : XWidget(parent)
{
    // 按键事件 → 转发给焦点组件
    Connect(this, MovementType::KeyPressed, this, [this](const XMovement& e) {
        auto& kp = dynamic_cast<const KeyPressed&>(e);
        for (auto* comp : m_Components) {
            if (comp->IsVisible() && comp->IsFocused()) {
                comp->OnKeyDown(kp.GetKeyCode());
                break;
            }
        }
    });

    Connect(this, MovementType::KeyTyped, this, [this](const XMovement& e) {
        auto& kt = dynamic_cast<const KeyTyped&>(e);
        for (auto* comp : m_Components) {
            if (comp->IsVisible() && comp->IsFocused()) {
                comp->OnChar((wchar_t)kt.GetKeyCode());
                break;
            }
        }
    });

    // 鼠标滚轮 → 转发给命中的组件（ScrollArea 等）
    Connect(this, MovementType::MouseScrolled, this, [this](const XMovement& e) {
        auto& ms = dynamic_cast<const MouseScrolled&>(e);
        int sx = 0, sy = 0;
        GetMouseScreenPos(sx, sy);
        ScreenToClient(sx, sy);   // 已返回逻辑坐标

        Component* hit = HitTest(sx, sy);
        if (hit && hit->IsVisible()) {
            hit->OnScroll(ms.GetYOffset());
        }
    });

    // 鼠标按下 → hit-test、设焦点、开始组件交互（可能拖动）
    Connect(this, MovementType::MouseButtonPressed, this, [this](const XMovement& e) {
        int sx = 0, sy = 0;
        GetMouseScreenPos(sx, sy);
        ScreenToClient(sx, sy);   // 已返回逻辑坐标

        // 旧焦点取消
        for (auto* comp : m_Components) {
            if (comp->IsFocused()) {
                comp->SetFocused(false);
                break;
            }
        }

        // 新焦点 + 记录交互目标 + 转发坐标

        Component* hit = HitTest(sx, sy);
        if (hit) {
            hit->SetFocused(true);
            m_DragTarget = hit;
            m_DragStartX = sx;
            m_DragStartY = sy;
            hit->OnMousePressed(sx - hit->GetX(), sy - hit->GetY());
            // 开始交互（拖滑块等）：捕获鼠标，避免移出窗口后 move/up 事件丢失
            CaptureMouse();
        }
        else {
            m_DragTarget = nullptr;
        }

        // 焦点变化（获得/失去）会改变组件外观（如 TextInput 光标立即出现），请求重绘
        RequestRepaint();
    });

    // 鼠标移动 → 若正在交互（拖动中）持续通知目标；否则 hover 通知命中组件

    Connect(this, MovementType::MouseMoved, this, [this](const XMovement& e) {
        int sx = 0, sy = 0;
        GetMouseScreenPos(sx, sy);
        ScreenToClient(sx, sy);   // 已返回逻辑坐标

        if (m_DragTarget) {
            m_DragTarget->OnMouseMoved(sx - m_DragTarget->GetX(), sy - m_DragTarget->GetY());
        }
        else {
            Component* hit = HitTest(sx, sy);
            if (hit && hit->IsVisible())
                hit->OnMouseMoved(sx - hit->GetX(), sy - hit->GetY());
        }
    });

    // 鼠标抬起 → 结束组件交互
    Connect(this, MovementType::MouseButtonReleased, this, [this](const XMovement& e) {
        int sx = 0, sy = 0;
        GetMouseScreenPos(sx, sy);
        ScreenToClient(sx, sy);   // 已返回逻辑坐标

        if (m_DragTarget) {
            m_DragTarget->OnMouseReleased(sx - m_DragTarget->GetX(), sy - m_DragTarget->GetY());
            m_DragTarget = nullptr;
        }
        // 交互结束，释放鼠标捕获（即使松手发生在窗口外也保证触发）
        ReleaseMouseCapture();
    });
}

Container::~Container() {
    disConnect(this);
    ClearComponents();
}

void Container::AddComponent(Component* comp) {
    if (comp) {
        // 注入重绘回调：组件 RequestRepaint() → 请求所属窗口重绘
        comp->SetRepaintCallback([this]() { RequestRepaint(); });
        m_Components.push_back(comp);
    }
}

void Container::RemoveComponent(Component* comp) {
    auto it = std::find(m_Components.begin(), m_Components.end(), comp);
    if (it != m_Components.end()) {
        m_Components.erase(it);
    }
}

void Container::ClearComponents() {
    m_Components.clear();
}

Component* Container::HitTest(int x, int y) {
    for (auto it = m_Components.rbegin(); it != m_Components.rend(); ++it) {
        Component* comp = *it;
        int cx = comp->GetX();
        int cy = comp->GetY();
        int cw = comp->GetWidth();
        int ch = comp->GetHeight();
        if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
            return comp;
        }
    }
    return nullptr;
}

void Container::OnPaint(Canvas& canvas) {
    for (auto* comp : m_Components) {
        if (comp->IsVisible()) {
            canvas.SetClip(
                comp->GetX(), comp->GetY(),
                comp->GetWidth(), comp->GetHeight()
            );
            comp->OnPaint(canvas);
            canvas.ResetClip();
        }
    }
}

void Container::OnPaint(Canvas* canvas) {
    if (canvas) {
        OnPaint(*canvas);
    }
}

} 
// namespace X_Y
