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
            // 注入重绘回调：组件 RequestRepaint() → 通知宿主刷屏
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
    }

    void Panel::ClearComponents()
    {
        m_Components.clear();
    }

    Component *Panel::HitTest(int x, int y)
    {
        for (auto it = m_Components.rbegin(); it != m_Components.rend(); ++it)
        {
            Component *comp = *it;
            int cx = comp->GetX();
            int cy = comp->GetY();
            int cw = comp->GetWidth();
            int ch = comp->GetHeight();
            if (x >= cx && x < cx + cw && y >= cy && y < cy + ch)
                return comp;
        }
        return nullptr;
    }

    // ── 输入转发：坐标相对本 Panel，由宿主算好传入 ──
    void Panel::DispatchMousePressed(int localX, int localY)
    {
        // 旧焦点取消
        for (auto *comp : m_Components)
        {
            if (comp->IsFocused())
            {
                comp->SetFocused(false);
                break;
            }
        }

        Component *hit = HitTest(localX, localY);
        if (hit)
        {
            hit->SetFocused(true);
            m_DragTarget = hit;
            m_DragTargetVisible = true;
            hit->DispatchMousePressed(localX - hit->GetX(), localY - hit->GetY());
        }
        else
        {
            m_DragTarget = nullptr;
            m_DragTargetVisible = false;
        }

        // 焦点变化会改变组件外观（如 TextInput 光标），请求宿主重绘
        RequestRepaint();
    }

    void Panel::DispatchMouseMoved(int localX, int localY)
    {
        if (m_DragTarget && m_DragTargetVisible)
        {
            m_DragTarget->DispatchMouseMoved(localX - m_DragTarget->GetX(),
                                            localY - m_DragTarget->GetY());
        }
        else
        {
            Component *hit = HitTest(localX, localY);
            if (hit && hit->IsVisible())
                hit->DispatchMouseMoved(localX - hit->GetX(), localY - hit->GetY());
        }
    }

    void Panel::DispatchMouseReleased(int localX, int localY)
    {
        if (m_DragTarget && m_DragTargetVisible)
        {
            m_DragTarget->DispatchMouseReleased(localX - m_DragTarget->GetX(),
                                               localY - m_DragTarget->GetY());
            m_DragTarget = nullptr;
            m_DragTargetVisible = false;
        }
    }

    void Panel::DispatchKeyDown(Input_t::KeyCode key)
    {
        for (auto *comp : m_Components)
        {
            if (comp->IsVisible() && comp->IsFocused())
            {
                comp->DispatchKeyDown(key);
                break;
            }
        }
    }

    void Panel::DispatchChar(wchar_t ch)
    {
        for (auto *comp : m_Components)
        {
            if (comp->IsVisible() && comp->IsFocused())
            {
                comp->DispatchChar(ch);
                break;
            }
        }
    }

    void Panel::DispatchMouseScrolled(int localX, int localY, float yDelta)
    {
        Component *hit = HitTest(localX, localY);
        if (hit && hit->IsVisible())
            hit->OnScroll(yDelta);
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

} // namespace X_Y
