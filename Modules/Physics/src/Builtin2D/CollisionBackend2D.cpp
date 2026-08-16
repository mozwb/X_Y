// ════════════════════════════════════════════════════════════
// X_Y::Physics — 后端实现：内置 2D 分层碰撞检测
//
//   L1 包围圆粗筛（O(1)）：距离 > 半径和 → 不相交，过滤大多数
//   L2 关键点三角形包含（廉价）：取中心点+顶点均匀采样，测是否落在
//       对方耳切三角剖分出的三角形集合内部。剖分离线，运行时零开销
//   L3 高速穿透（补隧道效应）：中心点扫掠线段(prevPos→pos) 是否穿过对方三角形
//       仅当 L2 未命中时触发
//
//   形状 = 有序多边形顶点（逆/顺时针皆可，body 内部统一逆时针），
//          设置时离线耳切剖分缓存。
// ════════════════════════════════════════════════════════════

#include "CollisionBackend2D.h"
#include "BodyImpl2D.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace X_Y::Physics {

namespace {

// ── AABB ──
struct Box2D { float minX, minY, maxX, maxY; };

// 扫掠矩形：覆盖 min(prev,cur)..max(prev,cur) 并外扩 r。
//   未动(prev==cur) 时退化为以 cur 为中心的静态矩形。
inline Box2D SweptBox(const Vec& prev, const Vec& cur, float r)
{
    return { std::min(prev.x, cur.x) - r, std::min(prev.y, cur.y) - r,
             std::max(prev.x, cur.x) + r, std::max(prev.y, cur.y) + r };
}

// 两 AABB 是否重叠（O(1)）
inline bool BoxesOverlap(const Box2D& a, const Box2D& b)
{
    return !(a.maxX < b.minX || b.maxX < a.minX ||
             a.maxY < b.minY || b.maxY < a.minY);
}

// ── 几何工具 ──
inline float Cross(const Vec& o, const Vec& a, const Vec& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// 点是否在三角形(a,b,c)内（含边）
bool PointInTri(const Vec& p, const Vec& a, const Vec& b, const Vec& c)
{
    float d1 = Cross(a, b, p);
    float d2 = Cross(b, c, p);
    float d3 = Cross(c, a, p);
    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

// 点是否落在某三角形的集合内（任一眼中；三角形经 body 旋转+平移）
bool PointInAnyTri(const Vec& p, const BodyImpl& body,
                   const std::vector<Tri2D>& tris)
{
    for (const auto& t : tris) {
        Vec wa = body.LocalToWorld(t.v[0]);
        Vec wb = body.LocalToWorld(t.v[1]);
        Vec wc = body.LocalToWorld(t.v[2]);
        if (PointInTri(p, wa, wb, wc)) return true;
    }
    return false;
}

// 点是否在线段 p1-p2 上（共线前提）
bool OnSegment(const Vec& p, const Vec& p1, const Vec& p2)
{
    return (p.x >= std::min(p1.x, p2.x) - 1e-6f && p.x <= std::max(p1.x, p2.x) + 1e-6f &&
            p.y >= std::min(p1.y, p2.y) - 1e-6f && p.y <= std::max(p1.y, p2.y) + 1e-6f);
}

// 线段 s1-s2 与 t1-t2 是否相交
bool SegmentsIntersect(const Vec& s1, const Vec& s2,
                       const Vec& t1, const Vec& t2)
{
    float d1 = Cross(t1, t2, s1);
    float d2 = Cross(t1, t2, s2);
    float d3 = Cross(s1, s2, t1);
    float d4 = Cross(s1, s2, t2);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
        return true;
    if (std::abs(d1) < 1e-6f && OnSegment(s1, t1, t2)) return true;
    if (std::abs(d2) < 1e-6f && OnSegment(s2, t1, t2)) return true;
    if (std::abs(d3) < 1e-6f && OnSegment(t1, s1, s2)) return true;
    if (std::abs(d4) < 1e-6f && OnSegment(t2, s1, s2)) return true;
    return false;
}

// 线段 (p0→p1) 是否穿过三角形集合内任一边（命中三角形边界/内部；三角形经 body 旋转+平移）
bool SegmentHitsAnyTri(const Vec& p0, const Vec& p1, const BodyImpl& body,
                       const std::vector<Tri2D>& tris)
{
    for (const auto& t : tris)
    {
        Vec a = body.LocalToWorld(t.v[0]);
        Vec b = body.LocalToWorld(t.v[1]);
        Vec c = body.LocalToWorld(t.v[2]);
        // 扫掠终点落在三角形内 → 直接命中（或穿透）
        if (PointInTri(p1, a, b, c)) return true;
        // 扫掠线段与三角形任一边相交
        if (SegmentsIntersect(p0, p1, a, b)) return true;
        if (SegmentsIntersect(p0, p1, b, c)) return true;
        if (SegmentsIntersect(p0, p1, c, a)) return true;
    }
    return false;
}

// 点 p 是否命中 body 的“内部”：
//   body 是圆(points 空) → 距离 <= 半径（O(1)）
//   body 是多边形         → 落在剖分三角形内
// 统一 L2 的内部测试分发，圆/多边形一视同仁。
bool PointHitsBody(const Vec& p, const BodyImpl& body)
{
    if (body.IsCircle())
        return MATH::Distance(p, body.GetPos()) <= body.GetBoundingRadius() + 1e-6f;
    return PointInAnyTri(p, body, body.GetTriangles());
}

} // namespace

bool CollisionBackend2D::Intersect(const BodyImpl& a, const BodyImpl& b) const
{
    // ── L1 粗筛：扫掠矩形（AABB）不相交 → 排除（O(1)，含高速穿透路径）──
    float ra = a.GetBoundingRadius();
    float rb = b.GetBoundingRadius();
    Box2D ba = SweptBox(a.GetPrevPos(), a.GetPos(), ra);
    Box2D bb = SweptBox(b.GetPrevPos(), b.GetPos(), rb);
    if (!BoxesOverlap(ba, bb))
        return false;   // 路径 + 包围盒完全分离，绝不相交

    // 圆 vs 圆：中心距 <= 半径和即精确命中
    if (a.IsCircle() && b.IsCircle())
        return MATH::Distance(a.GetPos(), b.GetPos()) <= ra + rb + 1e-6f;

    // ── L2 关键点互测（中心 + 均匀采样顶点 → 对方“内部”，圆/多边形统一分发）──
    constexpr int kMaxKeyPoints = 8;
    auto keysA = a.SampleWorldKeyPoints(kMaxKeyPoints);
    auto keysB = b.SampleWorldKeyPoints(kMaxKeyPoints);

    // A 的关键点 命中 B 的内部；B 的关键点 命中 A 的内部（对称）
    for (const auto& k : keysA)
        if (PointHitsBody(k, b)) return true;
    for (const auto& k : keysB)
        if (PointHitsBody(k, a)) return true;

    // ── L3 高速穿透（L2 未命中才跑）：主体扫掠线段 vs 对方内部 ──
    // 只有对方是多边形才有三角形可穿（圆 vs 圆已由中心距覆盖；圆无穿透面）
    if (!b.IsCircle())
        if (SegmentHitsAnyTri(a.GetPrevPos(), a.GetPos(), b, b.GetTriangles()))
            return true;
    if (!a.IsCircle())
        if (SegmentHitsAnyTri(b.GetPrevPos(), b.GetPos(), a, a.GetTriangles()))
            return true;

    return false;
}

} // namespace X_Y::Physics
