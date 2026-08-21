#pragma once
#include "Component.h"
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <vector>

namespace X_Y
{
    class Horizontal final : public Component
    {
    public:
        using SelectionCallback = std::function<void(std::size_t, Component *)>;

        void AddComponent(Component *component);
        void AddComponents(std::initializer_list<Component *> components);
        void RemoveComponent(Component *component);
        void Clear();
        void SetGap(int gap);
        int GetGap() const { return Gap; }
        std::size_t GetComponentCount() const { return Components.size(); }
        Component *GetComponent(std::size_t index) const;
        std::size_t GetSelectedIndex() const { return SelectedIndex; }
        void Select(std::size_t index);

        SelectionCallback OnSelected;

        void OnPaint(Canvas &canvas) override;
        void OnMousePressed(int localX, int localY) override;
        void OnMouseMoved(int localX, int localY) override;
        void OnMouseReleased(int localX, int localY) override;

    private:
        static constexpr std::size_t npos = static_cast<std::size_t>(-1);
        void LayoutComponents();
        Component *HitTest(int x, int y, std::size_t &index) const;

        std::vector<Component *> Components;
        std::size_t SelectedIndex = npos;
        int Gap = 0;
    };
}