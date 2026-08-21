#include "Component/horizontal.h"
#include <algorithm>

namespace X_Y
{
    void Horizontal::AddComponent(Component *component)
    {
        if (!component)
            return;

        component->SetRepaintCallback([this]()
                                      { RequestRepaint(); });
        Components.push_back(component);
        LayoutComponents();
        RequestRepaint();
    }

    void Horizontal::AddComponents(std::initializer_list<Component *> components)
    {
        for (Component *component : components)
            AddComponent(component);
    }

    void Horizontal::RemoveComponent(Component *component)
    {
        auto it = std::find(Components.begin(), Components.end(), component);
        if (it == Components.end())
            return;

        const std::size_t removedIndex = static_cast<std::size_t>(it - Components.begin());
        Components.erase(it);
        if (SelectedIndex == removedIndex)
            SelectedIndex = npos;
        else if (SelectedIndex != npos && SelectedIndex > removedIndex)
            --SelectedIndex;

        LayoutComponents();
        RequestRepaint();
    }

    void Horizontal::Clear()
    {
        Components.clear();
        SelectedIndex = npos;
        RequestRepaint();
    }

    void Horizontal::SetGap(int gap)
    {
        Gap = std::max(0, gap);
        LayoutComponents();
        RequestRepaint();
    }

    Component *Horizontal::GetComponent(std::size_t index) const
    {
        return index < Components.size() ? Components[index] : nullptr;
    }

    void Horizontal::Select(std::size_t index)
    {
        if (index >= Components.size())
            return;

        SelectedIndex = index;
        for (std::size_t i = 0; i < Components.size(); ++i)
            Components[i]->SetFocused(i == SelectedIndex);

        if (OnSelected)
            OnSelected(index, Components[index]);
        RequestRepaint();
    }

    void Horizontal::LayoutComponents()
    {
        int x = 0;
        for (Component *component : Components)
        {
            component->SetRect(x, 0, component->GetWidth(), GetHeight());
            x += component->GetWidth() + Gap;
        }
    }

    Component *Horizontal::HitTest(int x, int y, std::size_t &index) const
    {
        for (std::size_t i = 0; i < Components.size(); ++i)
        {
            Component *component = Components[i];
            if (component->IsVisible() && x >= component->GetX() &&
                x < component->GetX() + component->GetWidth() &&
                y >= component->GetY() && y < component->GetY() + component->GetHeight())
            {
                index = i;
                return component;
            }
        }
        index = npos;
        return nullptr;
    }

    void Horizontal::OnPaint(Canvas &canvas)
    {
        LayoutComponents();
        for (Component *component : Components)
        {
            if (!component->IsVisible())
                continue;

            canvas.SetClip(component->GetX(), component->GetY(),
                           component->GetWidth(), component->GetHeight());
            component->OnPaint(canvas);
            canvas.ResetClip();
        }
    }

    void Horizontal::OnMousePressed(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (!component)
            return;

        Select(index);
        component->OnMousePressed(localX - component->GetX(), localY - component->GetY());
    }

    void Horizontal::OnMouseMoved(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (component)
            component->OnMouseMoved(localX - component->GetX(), localY - component->GetY());
    }

    void Horizontal::OnMouseReleased(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (component)
            component->OnMouseReleased(localX - component->GetX(), localY - component->GetY());
    }

    void Horizontal::DispatchKeyDown(Input_t::KeyCode key)
    {
        for (Component *component : Components)
            if (component->IsVisible() && component->IsFocused())
            {
                component->DispatchKeyDown(key);
                return;
            }
        OnKeyDown(key);
    }

    void Horizontal::DispatchChar(wchar_t ch)
    {
        for (Component *component : Components)
            if (component->IsVisible() && component->IsFocused())
            {
                component->DispatchChar(ch);
                return;
            }
        OnChar(ch);
    }

    void Horizontal::DispatchMousePressed(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (!component)
        {
            OnMousePressed(localX, localY);
            return;
        }

        Select(index);
        component->DispatchMousePressed(localX - component->GetX(), localY - component->GetY());
    }

    void Horizontal::DispatchMouseMoved(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (component)
            component->DispatchMouseMoved(localX - component->GetX(), localY - component->GetY());
        else
            OnMouseMoved(localX, localY);
    }

    void Horizontal::DispatchMouseReleased(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (component)
            component->DispatchMouseReleased(localX - component->GetX(), localY - component->GetY());
        else
            OnMouseReleased(localX, localY);
    }
}