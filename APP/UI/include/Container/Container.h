#pragma once
#include "Widget/include/XWidget.h"
#include "Widget/include/Canvas.h"
#include <vector>

namespace X_Y {

    class Component;

    class Container : public XWidget {
    public:
        explicit Container(XWidget* parent = nullptr);
        virtual ~Container();

        void AddComponent(Component* comp);
        void RemoveComponent(Component* comp);
        void ClearComponents();

    protected:
        // 平台绘制回调
        void OnPaint(Canvas* canvas) override;

        // 组件绘制（基类版本）
        virtual void OnPaint(Canvas& canvas);
        Component* HitTest(int x, int y);
        std::vector<Component*> m_Components;

    private:
        // 拖拽等需要持续跟踪的交互目标（按下到抬起期间保持）

        Component* m_DragTarget = nullptr;
        int m_DragStartX = 0, m_DragStartY = 0;
    };

}
