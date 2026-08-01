#pragma once
#include "Widget/include/Canvas.h"
#include "Input/include/Input.h"
#include<functional>
namespace X_Y {

    class Component {
    public:
        Component() = default;
        virtual ~Component() = default;

        void SetRect(int x, int y, int w, int h) {
            m_X = x; m_Y = y; m_W = w; m_H = h;
        }
        int GetX() const { return m_X; }
        int GetY() const { return m_Y; }
        int GetWidth() const { return m_W; }
        int GetHeight() const { return m_H; }

        void SetVisible(bool v) { m_Visible = v; }
        bool IsVisible() const { return m_Visible; }

        void SetMouseLocal(int x, int y) { m_MouseX = x; m_MouseY = y; }
        int GetMouseLocalX() const { return m_MouseX; }
        int GetMouseLocalY() const { return m_MouseY; }

        void SetFocused(bool f) { m_Focused = f; }
        bool IsFocused() const { return m_Focused; }

        virtual void OnKeyDown(Input_t::KeyCode key) {}
        virtual void OnChar(wchar_t ch) {}

        // 通用滚动输入：yDelta > 0 向上滚，< 0 向下滚（单位=1格）
        virtual void OnScroll(float yDelta) {}

        // 内容告诉视口：滚动一步的像素粒度（如列表=行高，普通内容=1）
        virtual int GetScrollStep() const { return 1; }

        // 鼠标交互（localX/localY 为相对本组件的局部坐标）

        virtual void OnMousePressed(int localX, int localY) {}
        virtual void OnMouseMoved(int localX, int localY) {}
        virtual void OnMouseReleased(int localX, int localY) {}

        // 请求所属窗口重绘（由 Container 在 AddComponent 时注入实现）

        void RequestRepaint() { if (m_RepaintCallback) m_RepaintCallback(); }
        void SetRepaintCallback(std::function<void()> cb) { m_RepaintCallback = std::move(cb); }

        virtual void OnPaint(Canvas& canvas) = 0;

    private:
        int m_X = 0, m_Y = 0, m_W = 100, m_H = 30;
        int m_MouseX = 0, m_MouseY = 0;
        bool m_Visible = true;
        bool m_Focused = false;
        std::function<void()> m_RepaintCallback;
    };

}
