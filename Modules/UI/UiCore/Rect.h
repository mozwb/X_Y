#pragma once

namespace X_Y
{

// 简单的整数矩形，供 UI 各层命中判定 + 坐标平移。
// 纯数据 + 内联方法，不引入虚函数。
struct Rect
{
    int x = 0, y = 0, w = 0, h = 0;

    bool Contains(int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    // 平移：产生一个相对 dx,dy 的新矩形（用于把坐标下传时扣掉本层偏移）
    Rect Offset(int dx, int dy) const
    {
        return Rect{ x + dx, y + dy, w, h };
    }

    int GetX() const { return x; }
    int GetY() const { return y; }
    int GetWidth() const { return w; }
    int GetHeight() const { return h; }
};

} // namespace X_Y
