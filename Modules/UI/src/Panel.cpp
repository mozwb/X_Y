#include "Panel/Panel.h"
#include <algorithm>

namespace X_Y
{

    void Panel::SetLayoutRect(int x, int y, int w, int h)
    {
        m_X = x;
        m_Y = y;
        m_W = w;
        m_H = h;
    }

    void Panel::GetLayoutRect(int &x, int &y, int &w, int &h) const
    {
        x = m_X;
        y = m_Y;
        w = m_W;
        h = m_H;
    }

    void Panel::AddComponent(Component *comp)
    {
        if (comp)
        {
            comp->SetRepaintCallback([this]()
                                    { RequestRepaint(); });
            m_Components.push_back(comp);
        }
    }

    void Panel::RemoveComponent(Component *comp)
    {
        auto it = std::find(m_Components.begin(), m_Components.end(), comp);
        if (it != m_Components.end())
            m_Components.erase(it);
        if (m_FocusedComponent == comp)
            m_FocusedComponent = nullptr;
        if (m_DragTarget == comp)
        {
            m_DragTarget = nullptr;
            m_DragTargetVisible = false;
        }
    }

    void Panel::ClearComponents()
    {
        m_Components.clear();
        m_FocusedComponent = nullptr;
        m_DragTarget = nullptr;
        m_DragTargetVisible = false;
    }

    Component *Panel::HitTest(int x, int y)
    {
        for (auto it = m_Components.rbegin(); it != m_Components.rend(); ++it)
        {
            Component *comp = *it;
            if (!comp->IsVisible())
                continue;
            int cx = comp->GetX();
            int cy = comp->GetY();
            int cw = comp->GetWidth();
            int ch = comp->GetHeight();
            if (x >= cx && x < cx + cw && y >= cy && y < cy + ch)
                return comp;
        }
        return nullptr;
    }

    void Panel::SetFocusedComponent(Component *comp)
    {
        if (m_FocusedComponent)
            m_FocusedComponent->SetFocused(false);
        m_FocusedComponent = comp;
        if (m_FocusedComponent)
            m_FocusedComponent->SetFocused(true);
    }

    // ── 输入命中路由：把事件对象下传给命中的组件（z 序），支持 Handled 冒泡 ──
    void Panel::OnInput(UIInputEvent &e)
    {
        // 鼠标类事件：命中组件，下传
        if (auto *me = dynamic_cast<UIMouseEvent *>(&e))
        {
            // 持续交互（拖滑块/press 后 move/up）：优先交给 m_DragTarget
            Component *target = (m_DragTarget && m_DragTargetVisible) ? m_DragTarget : nullptr;
            if (!target)
                target = HitTest(e.x, e.y);

            if (target)
            {
                if (me->action == MouseAction::Press)
                {
                    // 按下：设焦点 + 记拖拽目标
                    SetFocusedComponent(target);
                    m_DragTarget = target;
                    m_DragTargetVisible = true;
                }
                else if (me->action == MouseAction::Release)
                {
                    m_DragTarget = nullptr;
                    m_DragTargetVisible = false;
                }

                // 下传：把坐标转成组件局部坐标
                const int cx = target->GetX();
                const int cy = target->GetY();
                e.x -= cx;
                e.y -= cy;
                target->OnInput(e);
                e.x += cx;
                e.y += cy;
            }
            else if (me->action == MouseAction::Press)
            {
                SetFocusedComponent(nullptr); // 点空白 → 清焦点
            }
            return;
        }

        // 键盘焦点：喂给被聚焦的组件
        if (auto *ke = dynamic_cast<UIKeyEvent *>(&e))
        {
            if (m_FocusedComponent)
            {
                m_FocusedComponent->OnInput(e);
            }
            return;
        }
    }

    void Panel::OnPaint(Canvas &canvas)
    {
        for (auto *comp : m_Components)
        {
            if (comp->IsVisible())
            {
                canvas.SetClip(
                    m_X + comp->GetX(), m_Y + comp->GetY(),
                    comp->GetWidth(), comp->GetHeight());
                comp->OnPaint(canvas);
                canvas.ResetClip();
            }
        }
    }

    void Panel::FocusNext()
    {
        // 简易 tab 顺序：按添加顺序找下一个可见组件
        if (m_Components.empty())
            return;
        int start = 0;
        if (m_FocusedComponent)
        {
            auto it = std::find(m_Components.begin(), m_Components.end(), m_FocusedComponent);
            if (it != m_Components.end())
                start = (int)(it - m_Components.begin()) + 1;
        }
        const int n = (int)m_Components.size();
        for (int k = 0; k < n; ++k)
        {
            Component *c = m_Components[(start + k) % n];
            if (c->IsVisible())
            {
                SetFocusedComponent(c);
                return;
            }
        }
    }

} // namespace X_Y
