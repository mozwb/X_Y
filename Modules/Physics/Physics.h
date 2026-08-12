#pragma once
// ════════════════════════════════════════════════════════════
// X_Y::Physics — 微型物理（欧拉积分，给 MouseFlight 用）
// 数学用自研后端 XMath（验证后端可换）
// ════════════════════════════════════════════════════════════
#include "XMath/XMathCore.h"

namespace X_Y::Physics
{
    namespace MATH = X_Y::Math::XMath; // 用自研后端

    const float GRAVITY = 9.8f; // 重力加速度

    // 物理体：位置/速度/质量 + 积分
    struct Body
    {

        MATH::Vec2 pos;
        MATH::Vec2 vel;
        float mass = 1.0f;
        float unit = 1.0f; // 世界单位=多少米，默认 1 像素=1 米

        // 积分：按 dt 推进位置（速度不变时）
        void Integrate(float dt)
        {
            pos += vel * dt * unit;
        }

        // 施加力：F=ma，累加加速度入速度
        void ApplyForce(const MATH::Vec2 &force, float dt)
        {
            vel += force * (dt / mass);
        }

        // 施加重力
        void ApplyGravity(float dt)
        {
            vel += MATH::Vec2(0.0f, -GRAVITY) * dt;
        }
    };

}
