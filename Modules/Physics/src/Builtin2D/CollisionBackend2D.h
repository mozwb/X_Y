#pragma once
// ════════════════════════════════════════════════════════════
// X_Y::Physics — 后端实现：内置 2D 三层碰撞检测
//   L1 包围圆粗筛 → L2 线段/圆边相交（主检测）→ L3 点在内部（补包含）
// ════════════════════════════════════════════════════════════

#include "../../PhysicsImpl.h"

namespace X_Y::Physics {

class CollisionBackend2D : public CollisionBackend
{
public:
    bool Intersect(const BodyImpl& a, const BodyImpl& b) const override;
};

} // namespace X_Y::Physics
