#pragma once
#include "Input.h"
#ifdef XY_PLATFORM_WINDOWS
#include <windows.h>
#include <type_traits>

namespace X_Y {
    namespace InputMapping {

        // Traits：自动决定输出类型
        template<typename T>
        struct KeyTranslator;

        // 平台键 → 内部键
        //template<> struct KeyTranslator<UINT> { using Target = Input_t::KeyCode; };
        template<> struct KeyTranslator<WPARAM> { using Target = Input_t::KeyCode; };

        // 内部键 → 平台键
        template<> struct KeyTranslator<Input_t::KeyCode> { using Target = WPARAM; };

        // Traits：鼠标键双向映射
        template<typename T>
        struct MouseTranslator;

        // 平台鼠标键 → 内部键
        //template<> struct MouseTranslator<UINT> { using Target = Input_t::MouseCode; };
        template<> struct MouseTranslator<int> { using Target = Input_t::MouseCode; };

        // 内部鼠标键 → 平台键
        template<> struct MouseTranslator<Input_t::MouseCode> { using Target = int; };

        // =======================================================================
        // 键盘双向翻译（已有的部分）
        // =======================================================================
        template<typename T>
        auto Translate(T value)
        {
            using Target = typename KeyTranslator<T>::Target;
            using namespace Input_t;

            // 字母 A-Z
            if (value >= 'A' && value <= 'Z')
                return (Target)value;

            // 数字 0-9
            if (value >= '0' && value <= '9')
                return (Target)value;

            // 双向映射宏
#define MAP(PLATFORM, INNER) \
            if (value == PLATFORM) return (Target)INNER; \
            if (value == INNER)    return (Target)PLATFORM;

            MAP(VK_SPACE, Key::Space);
            MAP(VK_OEM_4, Key::LeftBracket);
            MAP(VK_OEM_6, Key::RightBracket);
            MAP(VK_OEM_1, Key::Semicolon);
            MAP(VK_OEM_7, Key::Apostrophe);
            MAP(VK_OEM_COMMA, Key::Comma);
            MAP(VK_OEM_PERIOD, Key::Period);
            MAP(VK_OEM_2, Key::Slash);
            MAP(VK_OEM_MINUS, Key::Minus);
            MAP(VK_OEM_PLUS, Key::Equal);
            MAP(VK_OEM_5, Key::Backslash);
            MAP(VK_OEM_3, Key::GraveAccent);

            MAP(VK_ESCAPE, Key::Escape);
            MAP(VK_TAB, Key::Tab);
            MAP(VK_RETURN, Key::Enter);
            MAP(VK_BACK, Key::Backspace);
            MAP(VK_INSERT, Key::Insert);
            MAP(VK_DELETE, Key::Delete);
            MAP(VK_HOME, Key::Home);
            MAP(VK_END, Key::End);
            MAP(VK_PRIOR, Key::PageUp);
            MAP(VK_NEXT, Key::PageDown);
            MAP(VK_UP, Key::Up);
            MAP(VK_DOWN, Key::Down);
            MAP(VK_LEFT, Key::Left);
            MAP(VK_RIGHT, Key::Right);
            MAP(VK_CAPITAL, Key::CapsLock);
            MAP(VK_SCROLL, Key::ScrollLock);
            MAP(VK_NUMLOCK, Key::NumLock);
            MAP(VK_LSHIFT, Key::LeftShift);
            MAP(VK_RSHIFT, Key::RightShift);
            MAP(VK_LCONTROL, Key::LeftControl);
            MAP(VK_RCONTROL, Key::RightControl);
            MAP(VK_LMENU, Key::LeftAlt);
            MAP(VK_RMENU, Key::RightAlt);

#undef MAP

            // F1-F24
            if constexpr (std::is_same_v<T, UINT> || std::is_same_v<T, WPARAM>) {
                if (value >= VK_F1 && value <= VK_F24)
                    return (Target)(Key::F1 + (value - VK_F1));
            }
            else {
                if (value >= Key::F1 && value <= Key::F24)
                    return (Target)(VK_F1 + (value - Key::F1));
            }

            return (Target)0;
        }

        // =======================================================================
        // 🔥 新增：鼠标双向翻译
        // =======================================================================
        template<typename T>
        auto TranslateMouse(T value)
        {
            using Target = typename MouseTranslator<T>::Target;
            using namespace Input_t;

#define MAP_MOUSE(PLATFORM, INNER) \
            if (value == PLATFORM) return (Target)INNER; \
            if (value == INNER)    return (Target)PLATFORM;

            MAP_MOUSE(VK_LBUTTON, Mouse::ButtonLeft);
            MAP_MOUSE(VK_RBUTTON, Mouse::ButtonRight);
            MAP_MOUSE(VK_MBUTTON, Mouse::ButtonMiddle);

#undef MAP_MOUSE

            return (Target)0;
        }

    } // namespace InputMapping
} // namespace X_Y
#endif