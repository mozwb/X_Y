// ════════════════════════════════════════════════════════════
// X_Y::Physics — Body 门面构造/移动 + 全局后端管理
// ════════════════════════════════════════════════════════════

#include "../Physics.h"
#include "../PhysicsFactory.h"

namespace X_Y::Physics {

// ───────── Body 构造/析构/移动 ─────────
Body::Body() : m_impl(CreateBodyImpl()) {}
Body::~Body() { delete m_impl; }

Body::Body(Body&& other) noexcept : m_impl(other.m_impl)
{
    other.m_impl = nullptr;
}

Body& Body::operator=(Body&& other) noexcept
{
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

// ───────── 全局碰撞后端（默认内置 2D 三层，可 SetCollisionBackend 换）─────────
namespace {

CollisionBackend* g_backend = nullptr;

CollisionBackend* EnsureBackend()
{
    if (!g_backend)
        g_backend = CreateCollisionBackend();
    return g_backend;
}

} // namespace

CollisionBackend* GetCollisionBackend()
{
    return EnsureBackend();
}

void SetCollisionBackend(CollisionBackend* backend)
{
    g_backend = backend;
}

} // namespace X_Y::Physics
