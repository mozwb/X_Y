#pragma once
#include "Widget/include/Canvas.h"
#include "Input/include/Input.h"

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

        virtual void OnPaint(Canvas& canvas) = 0;

    private:
        int m_X = 0, m_Y = 0, m_W = 100, m_H = 30;
        int m_MouseX = 0, m_MouseY = 0;
        bool m_Visible = true;
        bool m_Focused = false;
    };

}
