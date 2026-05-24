#include "window/src/Input/Input.h"
#include "window/src/Input/MapCode.h"
namespace X_Y {
    namespace Input_t {

        // 键盘是否按下（自动走双向翻译）
        bool Input::IsKeyPressed(KeyCode key)
        {
            UINT vk = InputMapping::Translate(key);
            if (vk == 0)
                return false;

            return (::GetKeyState(vk) & 0x8000) != 0;
        }

        // 鼠标是否按下（自动走鼠标双向翻译）
        bool Input::IsMouseButtonPressed(MouseCode button)
        {
            UINT vk = InputMapping::TranslateMouse(button);
            if (vk == 0)
                return false;

            return (::GetKeyState(vk) & 0x8000) != 0;
        }

        // 鼠标位置
        xpos Input::GetMousePosition()
        {
            POINT pt;
            ::GetCursorPos(&pt);
            return { (float)pt.x, (float)pt.y };
        }

        // 鼠标 X
        float Input::GetMouseX()
        {
            return (float)GetMousePosition().x;
        }

        // 鼠标 Y
        float Input::GetMouseY()
        {
            return (float)GetMousePosition().y;
        }

    } 
} 