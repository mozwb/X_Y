#include "Component/TextInput.h"

namespace X_Y {

    void TextInput::SetText(const char* text) {
        m_Text = text;
        m_CursorPos = (int)m_Text.size();
    }

    void TextInput::SetText(const std::string& text) {
        m_Text = text;
        m_CursorPos = (int)m_Text.size();
    }

    void TextInput::OnPaint(Canvas& canvas) {
        int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();

        canvas.FillRect(x, y, w, h, IsFocused() ? 0x00FFFFFF : 0x00F0F0F0);

        canvas.FillRect(x, y, w, 1, m_BorderColor);
        canvas.FillRect(x, y + h - 1, w, 1, m_BorderColor);
        canvas.FillRect(x, y, 1, h, m_BorderColor);
        canvas.FillRect(x + w - 1, y, 1, h, m_BorderColor);

        int textX = x + 4;
        int textY = y + (h - 14) / 2;
        if (!m_Text.empty()) {
            canvas.DrawText(textX, textY, m_Text.c_str(), m_TextColor);
        } else if (!this->IsFocused() && !m_Placeholder.empty()) {
            canvas.DrawText(textX, textY, m_Placeholder.c_str(), 0x00AAAAAA);
        }

        if (IsFocused()) {
            std::string before = m_Text.substr(0, m_CursorPos);
            int cx = textX + (int)before.size() * 8;
            canvas.FillRect(cx, textY, 1, 14, 0x00000000);
        }
    }

    void TextInput::OnKeyDown(int vk) {
        if (m_ReadOnly) return;

        switch (vk) {
            case VK_LEFT:
                if (m_CursorPos > 0) m_CursorPos--;
                break;
            case VK_RIGHT:
                if (m_CursorPos < (int)m_Text.size()) m_CursorPos++;
                break;
            case VK_HOME:
                m_CursorPos = 0;
                break;
            case VK_END:
                m_CursorPos = (int)m_Text.size();
                break;
            case VK_DELETE:
                if (m_CursorPos < (int)m_Text.size()) {
                    m_Text.erase(m_CursorPos, 1);
                    TriggerChange();
                }
                break;
            case VK_BACK:
                if (m_CursorPos > 0) {
                    m_CursorPos--;
                    m_Text.erase(m_CursorPos, 1);
                    TriggerChange();
                }
                break;
        }
    }

    void TextInput::OnChar(wchar_t ch) {
        if (m_ReadOnly) return;

        if (ch >= 32 && ch <= 126) {
            m_Text.insert(m_CursorPos, 1, (char)ch);
            m_CursorPos++;
            TriggerChange();
        }
    }

}
