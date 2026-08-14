#pragma once
// ════════════════════════════════════════════════════════════
// X_Y::Physics — 后端实现：2D 物理体（Vec2 数据）
//   实现中间层 BodyImpl 纯虚接口。换 3D = 新写 BodyImpl3D（Vec3），
//   上层/虚接口不用动。
//
//   几何模型（砚台定稿）：
//     pos      中心点
//     points   有序多边形顶点（逆/顺时针皆可，内部统一逆时针）
//     radius   包围圆半径（L1 粗筛）
//     triangles 耳切三角剖分结果（本地坐标，设置形状时离线算一次并缓存）
//     prevPos  上一帧位置（L3 高速穿透用）
// ════════════════════════════════════════════════════════════

#include "../../PhysicsImpl.h"
#include <vector>

namespace X_Y::Physics {

// 重力加速度
inline constexpr float kGravity = 9.8f;


class BodyImpl2D : public BodyImpl
{
public:
    BodyImpl2D();

    // ── 几何（变化即重剖 + 重置 prevPos）──
    void SetRadius(float r) override;
    void AddPoint(const Vec& p) override;
    void ClearGeometry() override;
    bool  IsCircle() const override;
    float GetBoundingRadius() const override;
    const std::vector<Vec>& GetLocalPoints() const override;
    std::vector<Vec> GetWorldPoints() const override;

    // ── 物理状态 ──
    void SetPos(const Vec& p) override; Vec GetPos() const override;
    void SetVel(const Vec& v) override; Vec GetVel() const override;
    void SetMass(float m) override; float GetMass() const override;
    void SetGravity(bool on) override; bool HasGravity() const override;
    void SetUnit(float u) override; float GetUnit() const override;

    // ── 物理演化 ──
    void Integrate(float dt) override;
    void ApplyForce(const Vec& f, float dt) override;
    void ApplyGravity(float dt) override;

    // ── 判定后端用 ──
    const std::vector<Tri2D>& GetTriangles() const override { return m_triangles; }
    Vec GetPrevPos() const override { return m_prevPos; }
    std::vector<Vec> SampleWorldKeyPoints(int maxSamples) const override;

private:
    // 离线：对 m_points 做耳切剖分（存本地 Trie），并更新 m_radius
    void RebuildEarClip();

    Vec   m_pos{0, 0};
    Vec   m_prevPos{0, 0};
    Vec   m_vel{0, 0};
    float m_mass    = 1.0f;
    float m_unit    = 1.0f;
    bool  m_gravity = true;

    float m_radius = 0.0f;
    std::vector<Vec>     m_points;
    std::vector<Tri2D>   m_triangles;    // 耳切结果（本地坐标）
};

} // namespace X_Y::Physics
