#include "Container/Container.h"
#include "Component/Component.h"
#include "Movement/include/KeyMovement.h"
#include "Movement/include/MouseMovement.h"

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

    // 鼠标按下 → hit-test 并设置焦点
    Connect(this, MovementType::MouseButtonPressed, this, [this](const XMovement& e) {
        auto& mb = dynamic_cast<const MouseButtonPressed&>(e);
        // 鼠标事件不是从这里发出的，暂不处理
        // 实际鼠标 hit-test 由 WM_LBUTTONDOWN 在平台层处理
    });
}

Container::~Container() {
    ClearComponents();
}

void Container::AddComponent(Component* comp) {
    if (comp) {
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

} // namespace X_Y
