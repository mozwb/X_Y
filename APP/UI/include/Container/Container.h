#pragma once
#include "Widget/include/XWidget.h"
#include "Canvas.h"
#include "Widget/include/WinCore.h"
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
        virtual void OnPaint(Canvas& canvas);
        Component* HitTest(int x, int y);
        std::vector<Component*> m_Components;

    private:
#ifdef XY_PLATFORM_WINDOWS
        static LRESULT CALLBACK ContainerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
    };

}
