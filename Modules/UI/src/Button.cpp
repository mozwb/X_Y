#include "Component/Button.h"
#include "Widget/FontLibrary.h"

namespace X_Y
{

    void Button::OnPaint(Canvas &canvas)
    {
        int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();

        uint32_t bg = m_MouseHover ? m_HoverColor : m_BgColor;

        if (m_Rounded)
            canvas.FillRoundRect(x + 1, y + 2, w - 2, h - 4, 6, bg);
        else
            canvas.FillRect(x, y, w, h, bg);

        if (!m_Rounded)
        {
            canvas.FillRect(x, y, w, 1, 0xFFCCCCCC);
            canvas.FillRect(x, y + h - 1, w, 1, 0xFFCCCCCC);
            canvas.FillRect(x, y, 1, h, 0xFFCCCCCC);
            canvas.FillRect(x + w - 1, y, 1, h, 0xFFCCCCCC);
        }

        int textX = x + (w - (int)m_Text.size() * 8) / 2;
        if (m_Closeable)
            textX -= 8;
        int textY = y + (h - 14) / 2;
        auto &font = FontLibrary::Instance().GetDefault();
        canvas.DrawText(font, textX, textY, m_Text.c_str(), m_TextColor);
        if (m_Closeable)
            canvas.DrawText(font, x + w - 22, textY, "x", m_TextColor);
    }

    void Button::OnInput(UIInputEvent &e)
    {
        auto *me = dynamic_cast<UIMouseEvent *>(&e);
        if (!me)
            return;
        const int localX = e.x, localY = e.y;

        if (me->action == MouseAction::Move)
        {
            bool hover = localX >= 0 && localX < GetWidth() &&
                         localY >= 0 && localY < GetHeight();
            if (hover != m_MouseHover)
            {
                m_MouseHover = hover;
                RequestRepaint();
            }
            return;
        }

        if (me->action == MouseAction::Press)
        {
            if (localX < 0 || localX >= GetWidth() || localY < 0 || localY >= GetHeight())
                return;
            if (m_Closeable && localX >= GetWidth() - 28)
            {
                if (m_OnClose)
                    m_OnClose();
                RequestRepaint();
            }
            else if (m_OnClick)
            {
                m_OnClick();
            }
            return;
        }
    }

}
