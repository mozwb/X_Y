#include "Component/Overlay.h"

namespace X_Y {

void Overlay::OnPaint(Canvas& canvas) {
    int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();
    if (w <= 0 || h <= 0) return;

    // 填充半透明背景
    canvas.FillRect(x, y, w, h, m_Color);

    // 如果边框宽度 > 0，画四边
    if (m_BorderWidth > 0) {
        canvas.FillRect(x, y, w, m_BorderWidth, m_BorderColor);
        canvas.FillRect(x, y + h - m_BorderWidth, w, m_BorderWidth, m_BorderColor);
        canvas.FillRect(x, y, m_BorderWidth, h, m_BorderColor);
        canvas.FillRect(x + w - m_BorderWidth, y, m_BorderWidth, h, m_BorderColor);
    }
}

}
