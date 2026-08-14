// ════════════════════════════════════════════════════════════
// X_Y::Physics — 后端实现：2D 物理体
//   - 形状设置时离线做耳切三角剖分，缓存三角形（运行时零剖分开销）
//   - Integrate 记录 prevPos 供 L3 高速穿透检测
// ════════════════════════════════════════════════════════════

#include "BodyImpl2D.h"
#include <algorithm>
#include <cmath>

namespace X_Y::Physics {

namespace {

// 2D 叉积
inline float Cross(const Vec& o, const Vec& a, const Vec& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// 多边形有向面积（正=逆时针, 负=顺时针）
float SignedArea(const std::vector<Vec>& pts)
{
    float s = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const Vec& a = pts[i];
        const Vec& b = pts[(i + 1) % pts.size()];
        s += a.x * b.y - b.x * a.y;
    }
    return s * 0.5f;
}

// 点 P 是否在三角形 (a,b,c) 内（含边界）
bool PointInTri(const Vec& p, const Vec& a, const Vec& b, const Vec& c)
{
    float d1 = Cross(a, b, p);
    float d2 = Cross(b, c, p);
    float d3 = Cross(c, a, p);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);   // 全部同号(或在边上) → 在内
}

// 耳切三角剖分（输入可为凹多边形，须不自交）。
// 返回三角形列表（本地坐标）。成功返回 true；退化/失败 false。
bool EarClipTriangulate(const std::vector<Vec>& input, std::vector<Tri2D>& outTris)
{
    if (input.size() < 3) return false;

    std::vector<Vec> pts = input;
    // 统一为逆时针
    if (SignedArea(pts) < 0.0f)
        std::reverse(pts.begin(), pts.end());

    outTris.clear();
    const int n0 = static_cast<int>(pts.size());
    // 用索引链表思想：维护一个"当前还在多边形里的索引"列表
    std::vector<int> idx(n0);
    for (int i = 0; i < n0; ++i) idx[i] = i;

    const bool ccw = true; // 已统一逆时针

    while (idx.size() > 3)
    {
        bool clipped = false;
        const int m = static_cast<int>(idx.size());
        for (int i = 0; i < m && idx.size() > 3; ++i)
        {
            int iPrev = (i - 1 + m) % m;
            int iNext = (i + 1) % m;
            const Vec& a = pts[idx[iPrev]];
            const Vec& b = pts[idx[i]];
            const Vec& c = pts[idx[iNext]];

            // b 必须是凸顶点（对逆时针多边形：Cross(a,b,c) > 0）
            if (ccw && Cross(a, b, c) <= 0.0f) continue;
            if (!ccw && Cross(a, b, c) >= 0.0f) continue;

            // 三角形 (a,b,c) 内不能含其它顶点（否则不是"耳"）
            bool containsOther = false;
            for (int j = 0; j < m; ++j)
            {
                if (j == i || j == iPrev || j == iNext) continue;
                if (PointInTri(pts[idx[j]], a, b, c)) { containsOther = true; break; }
            }
            if (containsOther) continue;

            // 切掉这个耳
            outTris.push_back(Tri2D{{a, b, c}});
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped)
            return false;   // 剖分失败（自交/退化）
    }

    if (idx.size() == 3)
    {
        outTris.push_back(Tri2D{{pts[idx[0]], pts[idx[1]], pts[idx[2]]}});
        return true;
    }
    return false;
}

} // namespace

BodyImpl2D::BodyImpl2D() {}

// ── 几何（变化即重剖）──
void BodyImpl2D::SetRadius(float r)
{
    m_radius = r;
    m_points.clear();
    m_triangles.clear();
    m_prevPos = m_pos;
}

void BodyImpl2D::AddPoint(const Vec& p)
{
    m_points.push_back(p);
    RebuildEarClip();
}

void BodyImpl2D::ClearGeometry()
{
    m_radius = 0.0f;
    m_points.clear();
    m_triangles.clear();
    m_prevPos = m_pos;
}

bool BodyImpl2D::IsCircle() const { return m_points.empty(); }

float BodyImpl2D::GetBoundingRadius() const
{
    if (IsCircle()) return m_radius;
    float maxR = 0.0f;
    for (const auto& p : m_points) {
        float r = MATH::Length(p);
        if (r > maxR) maxR = r;
    }
    return maxR;
}

// 离线耳切剖分 + 更新包围半径
void BodyImpl2D::RebuildEarClip()
{
    m_prevPos = m_pos;
    if (m_points.size() < 3) {
        m_triangles.clear();
        return;
    }
    EarClipTriangulate(m_points, m_triangles);

    float maxR = 0.0f;
    for (const auto& p : m_points) {
        float r = MATH::Length(p);
        if (r > maxR) maxR = r;
    }
    m_radius = maxR;
}

const std::vector<Vec>& BodyImpl2D::GetLocalPoints() const { return m_points; }

std::vector<Vec> BodyImpl2D::GetWorldPoints() const
{
    std::vector<Vec> out;
    out.reserve(m_points.size());
    for (const auto& p : m_points) out.push_back(m_pos + p);
    return out;
}

// L2 关键点采样：中心点 + 顶点均匀取（最多 maxSamples 个），世界坐标
std::vector<Vec> BodyImpl2D::SampleWorldKeyPoints(int maxSamples) const
{
    std::vector<Vec> out;
    if (maxSamples < 1) return out;
    out.reserve(static_cast<size_t>(maxSamples) + 1);

    out.push_back(m_pos);                     // 中心点

    if (m_points.empty()) return out;         // 圆：只有中心

    // 均匀采样顶点：达到上限时稀疏取
    size_t step = 1;
    if (m_points.size() > static_cast<size_t>(maxSamples - 1))
        step = (m_points.size() + maxSamples - 2) / (maxSamples - 1);
    for (size_t i = 0; i < m_points.size(); i += step) {
        out.push_back(m_pos + m_points[i]);
        if (out.size() >= static_cast<size_t>(maxSamples) + 1) break;
    }
    return out;
}

// ── 物理状态 ──
// SetPos：手动定位。prevPos 同步为新位置（无扫掠路径，表示静止/瞬移）。
// 真正的运动路径由 Integrate 记录（积分的下一跳用旧 pos 穿模探测）。
void BodyImpl2D::SetPos(const Vec& p) { m_pos = p; m_prevPos = p; }
Vec   BodyImpl2D::GetPos() const { return m_pos; }
void  BodyImpl2D::SetVel(const Vec& v) { m_vel = v; }
Vec   BodyImpl2D::GetVel() const { return m_vel; }
void  BodyImpl2D::SetMass(float m) { if (m > 0.0f) m_mass = m; }
float BodyImpl2D::GetMass() const { return m_mass; }
void  BodyImpl2D::SetGravity(bool on) { m_gravity = on; }
bool  BodyImpl2D::HasGravity() const { return m_gravity; }
void  BodyImpl2D::SetUnit(float u) { if (u > 0.0f) m_unit = u; }
float BodyImpl2D::GetUnit() const { return m_unit; }

// ── 物理演化（半隐式欧拉；记录 prevPos 供穿透）──
void BodyImpl2D::Integrate(float dt)
{
    m_prevPos = m_pos;
    m_pos += m_vel * dt * m_unit;
}

void BodyImpl2D::ApplyForce(const Vec& f, float dt)
{
    m_vel += f * (dt / m_mass);
}

void BodyImpl2D::ApplyGravity(float dt)
{
    if (m_gravity)
        m_vel += Vec(0.0f, -kGravity) * dt;
}

} // namespace X_Y::Physics
