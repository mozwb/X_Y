#pragma once
#include "Input.h"

namespace X_Y {

// 平台无关的键码映射接口
class KeyMapper {
public:
    virtual ~KeyMapper() = default;

    // 平台虚拟键 → 内部 KeyCode
    virtual uint32_t PlatformToKey(uint32_t platformKey) const = 0;
    // 内部 KeyCode → 平台虚拟键
    virtual uint32_t KeyToPlatform(uint32_t keyCode) const = 0;
    // 平台鼠标键 → 内部 MouseCode
    virtual uint32_t PlatformToMouse(uint32_t platformButton) const = 0;
    // 内部 MouseCode → 平台鼠标键
    virtual uint32_t MouseToPlatform(uint32_t mouseCode) const = 0;
    // 按键状态查询
    virtual bool IsKeyPressed(uint32_t keyCode) const = 0;
    // 鼠标状态查询
    virtual bool IsMousePressed(uint32_t mouseCode) const = 0;
    // 鼠标位置
    virtual void GetMousePos(float& x, float& y) const = 0;
};

// 平台工厂
class KeyMapperFactory {
public:
    static KeyMapper* Create();
};

}
// namespace X_Y
