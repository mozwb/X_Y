#pragma once
#include "Input.h"

namespace X_Y {

// 键码映射函数——跨平台声明
// 实际实现由 KeyMapper 平台层提供

namespace InputMapping {

    // 平台键 → 内部键（接收 uint32_t 类型键码）
    Input_t::KeyCode Translate(uint32_t platformKey);

    // 内部键 → 平台键
    uint32_t TranslateKey(Input_t::KeyCode key);

    // 平台鼠标键 → 内部鼠标键
    Input_t::MouseCode TranslateMouse(uint32_t platformButton);

    // 内部鼠标键 → 平台键
    uint32_t TranslateMouseKey(Input_t::MouseCode button);

}

}
