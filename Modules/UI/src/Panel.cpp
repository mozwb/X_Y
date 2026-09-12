#include "Panel/Panel.h"
#include "../UiCore/UINode.h"
#include <algorithm>

namespace X_Y
{

    // ── UINodeView：Panel 在 Dock 坐标系（= Dock 局部）里的矩形 ──
    UINodeView View(const Panel &node)
    {
        UINodeView v;
        v.self = Rect{ node.GetX(), node.GetY(), node.GetWidth(), node.GetHeight() };
        v.content = Rect{ 0, 0, v.self.w, v.self.h }; // 整块都是内容
        v.visible = true;
        return v;
    }

    // ── UINodeView：Component 在 Panel 坐标系（= Panel 局部）里的矩形 ──
    // Component 是叶子：没有子节点，content 即自身。
    UINodeView View(const Component &node)
    {
        UINodeView v;
        v.self = Rect{ node.GetX(), node.GetY(), node.GetWidth(), node.GetHeight() };
        v.content = Rect{ 0, 0, v.self.w, v.self.h };
        v.visible = node.IsVisible();
        return v;
    }

    // ⚠️ 坐标语义：x/y 是【宿主坐标系】里的位置 —— 由 Dock 调用时即 Dock 局部坐标。
    //    绝对坐标只活在 DockLayout/Dock 一侧，Panel 及以下全程相对。
    void Panel::SetLayoutRect(int x, int y, int w, int h)
    {
        m_X = x;
        m_Y = y;
        m_W = w;
        m_H = h;
        OnLayout();
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
        if (m_FocusedComponent == comp)
            return;

        if (m_FocusedComponent)
            m_FocusedComponent->SetFocused(false);
        m_FocusedComponent = comp;
        if (m_FocusedComponent)
            m_FocusedComponent->SetFocused(true);
        RequestRepaint();
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

    // Panel 把自己这棵树画到 canvas 上。
    // ★ 坐标约定：调用方（Dock）已 PushOrigin 到 Panel 的位置，所以本函数
    //   及所有组件一律按【Panel 局部坐标】(0,0 起画) —— 不再加 m_X/m_Y。
    //   组件也不需要知道自己在哪个 Panel/Dock/窗口里。
    void Panel::OnPaint(Canvas &canvas)
    {
        // ★ 外层裁剪：整个 Panel 的内容不许画到自己矩形之外。
        //   没有这道裁剪时，滚动中的内容（尤其 ScrollArea 里的列表/文字）
        //   会溢出到 Panel 外，盖住 Dock 的 tab 栏、越过分割线。
        canvas.SetClip(0, 0, m_W, m_H);

        for (auto *comp : m_Components)
        {
            if (comp->IsVisible())
            {
                canvas.SetClip(
                    comp->GetX(), comp->GetY(),
                    comp->GetWidth(), comp->GetHeight());
                comp->OnPaint(canvas);
                canvas.ResetClip();
                canvas.SetClip(0, 0, m_W, m_H); // 恢复外层裁剪
            }
        }

        canvas.ResetClip();
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
