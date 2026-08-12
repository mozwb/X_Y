#include "Component/Button.h"
#include "Widget/FontLibrary.h"

namespace X_Y {

    void Button::OnPaint(Canvas& canvas) {
        int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();

        uint32_t bg = m_MouseHover ? m_HoverColor : m_BgColor;

        canvas.FillRect(x, y, w, h, bg);

        canvas.FillRect(x, y, w, 1, 0xFFCCCCCC);
        canvas.FillRect(x, y + h - 1, w, 1, 0xFFCCCCCC);
        canvas.FillRect(x, y, 1, h, 0xFFCCCCCC);
        canvas.FillRect(x + w - 1, y, 1, h, 0xFFCCCCCC);

        int textX = x + (w - (int)m_Text.size() * 8) / 2;
        int textY = y + (h - 14) / 2;
        auto& font = FontLibrary::Instance().GetDefault();
        canvas.DrawText(font, textX, textY, m_Text.c_str(), m_TextColor);
    }

}
