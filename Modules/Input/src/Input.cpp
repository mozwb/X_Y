#include "Input.h"
#include "KeyMapper.h"

#ifdef XY_PLATFORM_WINDOWS
#include <windows.h>
#endif

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

void Input::SetMousePosition(float x, float y) {
    GetMapper()->SetMousePos(x, y);
}

// ── 设备层(全局)能力 ──────────────────────

void Input::SetCursorVisible(bool visible) {
#ifdef XY_PLATFORM_WINDOWS
    ::ShowCursor(visible ? TRUE : FALSE);
#endif
}

int Input::GetScreenWidth() {
#ifdef XY_PLATFORM_WINDOWS
    return ::GetSystemMetrics(SM_CXSCREEN);
#else
    return 0;
#endif
}

int Input::GetScreenHeight() {
#ifdef XY_PLATFORM_WINDOWS
    return ::GetSystemMetrics(SM_CYSCREEN);
#else
    return 0;
#endif
}

// ── 设备层：硬件直读轮询(转发到 KeyMapper 的 IsKeyDown/IsMouseDown) ──

bool Input::IsKeyDown(KeyCode key) {
    auto* mapper = GetMapper();
    return mapper->IsKeyDown(static_cast<uint32_t>(key));
}

bool Input::IsMouseDown(MouseCode button) {
    auto* mapper = GetMapper();
    return mapper->IsMouseDown(static_cast<uint32_t>(button));
}

} // namespace Input_t
} // namespace X_Y
