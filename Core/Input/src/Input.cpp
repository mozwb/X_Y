#include "Input.h"
#include "KeyMapper.h"

namespace X_Y {

// 全局 mapper 实例
static KeyMapper* s_Mapper = nullptr;

static KeyMapper* GetMapper() {
    if (!s_Mapper) {
        s_Mapper = KeyMapperFactory::Create();
    }
    return s_Mapper;
}

namespace Input_t {

bool Input::IsKeyPressed(KeyCode key) {
    auto* mapper = GetMapper();
    return mapper->IsKeyPressed(static_cast<uint32_t>(key));
}

bool Input::IsMouseButtonPressed(MouseCode button) {
    auto* mapper = GetMapper();
    return mapper->IsMousePressed(static_cast<uint32_t>(button));
}

xpos Input::GetMousePosition() {
    float x, y;
    GetMapper()->GetMousePos(x, y);
    return { x, y };
}

float Input::GetMouseX() {
    return GetMousePosition().x;
}

float Input::GetMouseY() {
    return GetMousePosition().y;
}

} // namespace Input_t
} // namespace X_Y
