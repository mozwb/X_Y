#include "MapCode.h"
#include "KeyMapper.h"
#include <windows.h>

namespace X_Y {

// ── 全局 KeyMapper 实例 ───────────────────────────
static KeyMapper* s_Mapper = nullptr;

// 懒初始化（在首次 Translate 调用时创建）
static KeyMapper* GetMapper() {
    if (!s_Mapper) {
        s_Mapper = KeyMapperFactory::Create();
    }
    return s_Mapper;
}

namespace InputMapping {

Input_t::KeyCode Translate(uint32_t platformKey) {
    auto* mapper = GetMapper();
    return static_cast<Input_t::KeyCode>(mapper->PlatformToKey(platformKey));
}

uint32_t TranslateKey(Input_t::KeyCode key) {
    auto* mapper = GetMapper();
    return mapper->KeyToPlatform(static_cast<uint32_t>(key));
}

Input_t::MouseCode TranslateMouse(uint32_t platformButton) {
    auto* mapper = GetMapper();
    return static_cast<Input_t::MouseCode>(mapper->PlatformToMouse(platformButton));
}

uint32_t TranslateMouseKey(Input_t::MouseCode button) {
    auto* mapper = GetMapper();
    return mapper->MouseToPlatform(static_cast<uint32_t>(button));
}

} // namespace InputMapping

// ── Win32 平台的 KeyMapper 实现 ─────────────────────

class KeyMapperWin32 : public KeyMapper {
public:
    uint32_t PlatformToKey(uint32_t platformKey) const override {
        using namespace Input_t;

        // 字母 A-Z
        if (platformKey >= 'A' && platformKey <= 'Z')
            return platformKey;
        // 数字 0-9
        if (platformKey >= '0' && platformKey <= '9')
            return platformKey;

#define MAP(PLATFORM, INNER) \
        if (platformKey == PLATFORM) return INNER;

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
        if (platformKey >= VK_F1 && platformKey <= VK_F24)
            return Key::F1 + (platformKey - VK_F1);

        return 0;
    }

    uint32_t KeyToPlatform(uint32_t keyCode) const override {
        using namespace Input_t;

        if (keyCode >= 'A' && keyCode <= 'Z')
            return keyCode;
        if (keyCode >= '0' && keyCode <= '9')
            return keyCode;

#define MAP(INNER, PLATFORM) \
        if (keyCode == INNER) return PLATFORM;

        MAP(Key::Space, VK_SPACE);
        MAP(Key::LeftBracket, VK_OEM_4);
        MAP(Key::RightBracket, VK_OEM_6);
        MAP(Key::Semicolon, VK_OEM_1);
        MAP(Key::Apostrophe, VK_OEM_7);
        MAP(Key::Comma, VK_OEM_COMMA);
        MAP(Key::Period, VK_OEM_PERIOD);
        MAP(Key::Slash, VK_OEM_2);
        MAP(Key::Minus, VK_OEM_MINUS);
        MAP(Key::Equal, VK_OEM_PLUS);
        MAP(Key::Backslash, VK_OEM_5);
        MAP(Key::GraveAccent, VK_OEM_3);
        MAP(Key::Escape, VK_ESCAPE);
        MAP(Key::Tab, VK_TAB);
        MAP(Key::Enter, VK_RETURN);
        MAP(Key::Backspace, VK_BACK);
        MAP(Key::Insert, VK_INSERT);
        MAP(Key::Delete, VK_DELETE);
        MAP(Key::Home, VK_HOME);
        MAP(Key::End, VK_END);
        MAP(Key::PageUp, VK_PRIOR);
        MAP(Key::PageDown, VK_NEXT);
        MAP(Key::Up, VK_UP);
        MAP(Key::Down, VK_DOWN);
        MAP(Key::Left, VK_LEFT);
        MAP(Key::Right, VK_RIGHT);
        MAP(Key::CapsLock, VK_CAPITAL);
        MAP(Key::ScrollLock, VK_SCROLL);
        MAP(Key::NumLock, VK_NUMLOCK);
        MAP(Key::LeftShift, VK_LSHIFT);
        MAP(Key::RightShift, VK_RSHIFT);
        MAP(Key::LeftControl, VK_LCONTROL);
        MAP(Key::RightControl, VK_RCONTROL);
        MAP(Key::LeftAlt, VK_LMENU);
        MAP(Key::RightAlt, VK_RMENU);
#undef MAP

        if (keyCode >= Key::F1 && keyCode <= Key::F24)
            return VK_F1 + (keyCode - Key::F1);

        return 0;
    }

    uint32_t PlatformToMouse(uint32_t platformButton) const override {
        using namespace Input_t;
        if (platformButton == VK_LBUTTON) return Mouse::ButtonLeft;
        if (platformButton == VK_RBUTTON) return Mouse::ButtonRight;
        if (platformButton == VK_MBUTTON) return Mouse::ButtonMiddle;
        return 0;
    }

    uint32_t MouseToPlatform(uint32_t mouseCode) const override {
        using namespace Input_t;
        if (mouseCode == Mouse::ButtonLeft)  return VK_LBUTTON;
        if (mouseCode == Mouse::ButtonRight) return VK_RBUTTON;
        if (mouseCode == Mouse::ButtonMiddle) return VK_MBUTTON;
        return 0;
    }

    bool IsKeyPressed(uint32_t keyCode) const override {
        uint32_t vk = KeyToPlatform(keyCode);
        if (vk == 0) return false;
        return (::GetKeyState((int)vk) & 0x8000) != 0;
    }

    bool IsMousePressed(uint32_t mouseCode) const override {
        uint32_t vk = MouseToPlatform(mouseCode);
        if (vk == 0) return false;
        return (::GetKeyState((int)vk) & 0x8000) != 0;
    }

    void GetMousePos(float& x, float& y) const override {
        POINT pt;
        ::GetCursorPos(&pt);
        x = (float)pt.x;
        y = (float)pt.y;
    }
};

// ── 工厂实现 ──────────────────────────────────────
KeyMapper* KeyMapperFactory::Create() {
    return new KeyMapperWin32();
}

} // namespace X_Y
