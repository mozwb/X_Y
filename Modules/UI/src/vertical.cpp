#include "Component/vertical.h"
#include <algorithm>

namespace X_Y
{
    void Vertical::AddComponent(Component *component)
    {
        if (!component)
            return;

        component->SetRepaintCallback([this]()
                                      { RequestRepaint(); });
        Components.push_back(component);
        LayoutComponents();
        RequestRepaint();
    }

    void Vertical::AddComponents(std::initializer_list<Component *> components)
    {
        for (Component *component : components)
            AddComponent(component);
    }

    void Vertical::RemoveComponent(Component *component)
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

    void Vertical::Clear()
    {
        Components.clear();
        SelectedIndex = npos;
        RequestRepaint();
    }

    void Vertical::SetGap(int gap)
    {
        Gap = std::max(0, gap);
        LayoutComponents();
        RequestRepaint();
    }

    Component *Vertical::GetComponent(std::size_t index) const
    {
        return index < Components.size() ? Components[index] : nullptr;
    }

    void Vertical::Select(std::size_t index)
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

    void Vertical::LayoutComponents()
    {
        int y = 0;
        for (Component *component : Components)
        {
            component->SetRect(0, y, GetWidth(), component->GetHeight());
            y += component->GetHeight() + Gap;
        }
    }

    Component *Vertical::HitTest(int x, int y, std::size_t &index) const
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

    void Vertical::OnPaint(Canvas &canvas)
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

    void Vertical::OnMousePressed(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (!component)
            return;

        Select(index);
        component->OnMousePressed(localX - component->GetX(), localY - component->GetY());
    }

    void Vertical::OnMouseMoved(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (component)
            component->OnMouseMoved(localX - component->GetX(), localY - component->GetY());
    }

    void Vertical::OnMouseReleased(int localX, int localY)
    {
        std::size_t index = npos;
        Component *component = HitTest(localX, localY, index);
        if (component)
            component->OnMouseReleased(localX - component->GetX(), localY - component->GetY());
    }
}