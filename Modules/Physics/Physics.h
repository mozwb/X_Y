#pragma once
// ════════════════════════════════════════════════════════════
// X_Y::Physics — 物理 + 碰撞门面（上层唯一接口，稳定不变）
//
// 三层架构（对齐 Widget 层）：
//   上层  本文件      稳定接口：Body 门面 + Test + SetBackend
//   中层  PhysicsImpl.h   纯虚接口 BodyImpl / CollisionBackend
//   下层  src/Builtin2D/  各后端具体实现（2D / 将来 3D / Box2D…）
//   工厂  PhysicsFactory  创建具体后端
//
// 用法（上层全程只碰 Body + Physics::Test）：
//   auto& body = player.body;
//   body.SetPos({0,0});
//   body.SetRadius(8);                 // 圆
//   // 或 body.AddPoint({0,0}); body.AddPoint({10,0}); body.AddPoint({5,8});
//   body.SetVel({0,0}); body.SetMass(1);
//
//   // 每帧检测可能碰的物体对：
//   X_Y::Physics::Test(a, b, [](Body& x, Body& y){ /* 碰撞处理 */ });
//
// Vec：点/向量类型别名（PhysicsImpl.h 定义）。当前 2D=Vec2，换 3D 改一处。
// ════════════════════════════════════════════════════════════

#include "PhysicsImpl.h"
#include "PhysicsFactory.h"

namespace X_Y::Physics
{

    // ───────── 全局碰撞后端（可换，默认内置三层）─────────
    CollisionBackend *GetCollisionBackend();
    void SetCollisionBackend(CollisionBackend *backend);

    // ───────── Body：Pimpl 门面（内部持 BodyImpl*，由工厂创建）─────────
    class Body
    {
    public:
        Body();
        ~Body();
        Body(Body &&other) noexcept;
        Body &operator=(Body &&other) noexcept;
        Body(const Body &) = delete; // 物理体不可拷贝
        Body &operator=(const Body &) = delete;

        // ── 几何（转中间虚接口 → 具体后端）──
        void SetRadius(float r) { m_impl->SetRadius(r); }
        void AddPoint(const Vec &p) { m_impl->AddPoint(p); }
        void ClearGeometry() { m_impl->ClearGeometry(); }
        bool IsCircle() const { return m_impl->IsCircle(); }
        float GetBoundingRadius() const { return m_impl->GetBoundingRadius(); }
        const std::vector<Vec> &GetLocalPoints() const { return m_impl->GetLocalPoints(); }
        std::vector<Vec> GetWorldPoints() const { return m_impl->GetWorldPoints(); }

        // ── 物理状态 ──
        void SetPos(const Vec &p) { m_impl->SetPos(p); }
        Vec GetPos() const { return m_impl->GetPos(); }
        void SetVel(const Vec &v) { m_impl->SetVel(v); }
        Vec GetVel() const { return m_impl->GetVel(); }
        void SetMass(float m) { m_impl->SetMass(m); }
        float GetMass() const { return m_impl->GetMass(); }
        void SetGravity(bool on) { m_impl->SetGravity(on); }
        bool HasGravity() const { return m_impl->HasGravity(); }
        void SetUnit(float u) { m_impl->SetUnit(u); }
        float GetUnit() const { return m_impl->GetUnit(); }
        // 朝向（弧度，逆时针为正；本地几何绕 pos 旋转）
        void SetRotation(float rad) { m_impl->SetRotation(rad); }
        float GetRotation() const { return m_impl->GetRotation(); }

        // ── 物理演化（半隐式欧拉）──
        void Integrate(float dt) { m_impl->Integrate(dt); }
        void ApplyForce(const Vec &f, float dt) { m_impl->ApplyForce(f, dt); }
        void ApplyGravity(float dt) { m_impl->ApplyGravity(dt); }

        // 内部访问（碰撞/容器用）
        BodyImpl *raw() { return m_impl; }
        const BodyImpl *raw() const { return m_impl; }

    private:
        BodyImpl *m_impl;
    };

    // ───────── 接触结果 ─────────
    struct Contact
    {
        Body *a = nullptr;
        Body *b = nullptr;
        bool hit = false;
        // 法线/穿透深度：需要反弹/推开时再加
    };

    // ───────── 主入口：检测 a、b 是否相撞，撞了调 handler(a,b) ─────────
    template <class Handler>
    void Test(Body &a, Body &b, Handler handler)
    {
        if (GetCollisionBackend()->Intersect(*a.raw(), *b.raw()))
            handler(a, b);
    }

} // namespace X_Y::Physics
