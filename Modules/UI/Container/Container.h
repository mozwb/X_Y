#pragma once
#include "Widget/XWidget.h"
#include "Widget/Canvas.h"
#include <vector>

namespace X_Y
{

    class Component;

    class Container : public XWidget
    {
    public:
        explicit Container(XWidget *parent = nullptr);
        virtual ~Container();

        void AddComponent(Component *comp);
        void RemoveComponent(Component *comp);
        void ClearComponents();

    protected:
        // 平台绘制回调
        // 窗口绘制有两种，如果是这种就是托管给消息循环绘制
        // 如果采用flush就是主动绘制
        void OnPaint(Canvas *canvas) override;
        Component *HitTest(int x, int y);
        std::vector<Component *> m_Components;

    private:
        // 拖拽等需要持续跟踪的交互目标（按下到抬起期间保持）

        Component *m_DragTarget = nullptr;
        int m_DragStartX = 0, m_DragStartY = 0;
    };

}
