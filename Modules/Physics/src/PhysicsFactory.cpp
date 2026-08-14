// ════════════════════════════════════════════════════════════
// X_Y::Physics — 工厂实现：创建具体后端
// ════════════════════════════════════════════════════════════

#include "../PhysicsFactory.h"
#include "Builtin2D/BodyImpl2D.h"
#include "Builtin2D/CollisionBackend2D.h"

namespace X_Y::Physics {

BodyImpl* CreateBodyImpl(PhysicsBackendType type)
{
    switch (type) {
    case PhysicsBackendType::Builtin2D:
    default:
        return new BodyImpl2D();
    }
}

CollisionBackend* CreateCollisionBackend(PhysicsBackendType type)
{
    switch (type) {
    case PhysicsBackendType::Builtin2D:
    default:
        return new CollisionBackend2D();
    }
}

} // namespace X_Y::Physics
