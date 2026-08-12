#pragma once
// ════════════════════════════════════════════════════════════
// X_Y::Math::XMath — 自研数学后端（不依赖 glm，边用边补）
//
// 当前进度：Vec2 起步（够用即可，写 MouseFlight 时按需补充）
// 待补：Vec3 / Vec4 / Mat3 / Mat4 / Quat / 常用函数
// ════════════════════════════════════════════════════════════

#include <cmath>

namespace X_Y::Math::XMath {

    // ── Vec2 ────────────────────────────────────────────────
    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;

        Vec2() = default;
        Vec2(float x_, float y_) : x(x_), y(y_) {}

        // 分量访问
        float& operator[](int i) { return i == 0 ? x : y; }
        const float& operator[](int i) const { return i == 0 ? x : y; }

        // 算术
        Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
        Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
        Vec2 operator*(float s) const { return { x * s, y * s }; }
        Vec2 operator/(float s) const { return { x / s, y / s }; }

        Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
        Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
        Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
        Vec2& operator/=(float s) { x /= s; y /= s; return *this; }

        bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
        bool operator!=(const Vec2& o) const { return !(*this == o); }
    };

    inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

    // ── 向量函数 ────────────────────────────────────────────
    inline float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
    inline float Length(const Vec2& v) { return std::sqrt(Dot(v, v)); }
    inline Vec2 Normalize(const Vec2& v) {
        float len = Length(v);
        return len > 0.0f ? v / len : Vec2();
    }
    inline float Distance(const Vec2& a, const Vec2& b) { return Length(a - b); }

    // 数乘快捷：Vec2 * Vec2 逐分量（可选）
    inline Vec2 Hadamard(const Vec2& a, const Vec2& b) { return { a.x * b.x, a.y * b.y }; }

    // ══ 待补：Vec3 / Vec4 / Mat3 / Mat4 / Quat / 角度转换等 ══

}
