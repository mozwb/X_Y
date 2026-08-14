#pragma once
// ════════════════════════════════════════════════════════════
// X_Y::Physics — 工厂：创建具体后端（对齐 Widget 的 PlatformFactory）
//   上层只依赖本工厂返回的纯虚接口指针，不感知具体实现类。
// ════════════════════════════════════════════════════════════

#include "PhysicsImpl.h"

namespace X_Y::Physics {

// 后端类型（当前仅 2D 内置；将来加 3D / Box2D）
enum class PhysicsBackendType
{
    Builtin2D,   // 内置 2D：Vec2 数据 + 三层碰撞算法
    // 将来: Builtin3D, Box2D, ...
};

// 创建物理体实现（按类型）
BodyImpl* CreateBodyImpl(PhysicsBackendType type = PhysicsBackendType::Builtin2D);

// 创建碰撞后端（按类型）
CollisionBackend* CreateCollisionBackend(PhysicsBackendType type = PhysicsBackendType::Builtin2D);

} // namespace X_Y::Physics
