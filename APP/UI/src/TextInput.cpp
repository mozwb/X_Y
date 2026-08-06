#include "Component/TextInput.h"
#include "Input/include/MapCode.h"

namespace X_Y {

    // ── 工具：宽字符 → UTF-8 ──
    // 输入框存 UTF-8（std::string），中文字符按 UTF-8 编码插入，不能强转单字节。

    std::string TextInput::Utf8FromWide(wchar_t ch) {
        if (ch < 0x80) {
            return std::string(1, (char)ch);
        }
        if (ch < 0x800) {
            char b[2] = {
                (char)(0xC0 | (ch >> 6)),
                (char)(0x80 | (ch & 0x3F)),
            };
            return std::string(b, 2);
        }
        char b[3] = {
            (char)(0xE0 | (ch >> 12)),
            (char)(0x80 | ((ch >> 6) & 0x3F)),
            (char)(0x80 | (ch & 0x3F)),
        };
        return std::string(b, 3);
    }

    void TextInput::SetText(const char* text) {
        m_Text = text ? text : "";
        m_CursorPos = (int)m_Text.size();
        RequestRepaint();
        NotifyTextChange();
    }

    void TextInput::SetText(const std::string& text) {
        m_Text = text;
        m_CursorPos = (int)m_Text.size();
        RequestRepaint();
        NotifyTextChange();
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
            // 用实际字体测宽（中文/全角宽度不同，不能硬编码 *8）
            int cx = textX + canvas.MeasureText(before.c_str());
            canvas.FillRect(cx, textY, 1, 14, 0x00000000);
        }
    }

    void TextInput::OnKeyDown(Input_t::KeyCode key) {
        using namespace Input_t;
        if (m_ReadOnly) return;

        bool changed = false;

        // 光标按 UTF-8 字符边界步进（不能裸 m_CursorPos++，会切入多字节字符中间）
        auto prevCharStart = [](const std::string& s, int pos) {
            if (pos <= 0) return 0;
            int p = pos - 1;
            while (p > 0 && ((unsigned char)s[p] & 0xC0) == 0x80)
                p--;
            return p;
        };
        auto nextCharEnd = [](const std::string& s, int pos) {
            if (pos >= (int)s.size()) return (int)s.size();
            int p = pos;
            p++;   // 越过首字节
            while (p < (int)s.size() && ((unsigned char)s[p] & 0xC0) == 0x80)
                p++;
            return p;
        };

        switch (key) {
            case Key::Left:
                m_CursorPos = prevCharStart(m_Text, m_CursorPos);
                break;
            case Key::Right:
                m_CursorPos = nextCharEnd(m_Text, m_CursorPos);
                break;
            case Key::Home:
                m_CursorPos = 0;
                break;
            case Key::Enter:
                // 回车：通知外部（如“添加到筛选规则”），不清空文本，由外部决定
                if (OnEnter) OnEnter();
                break;
            case Key::End:
                m_CursorPos = (int)m_Text.size();
                break;
            case Key::Delete: {
                if (m_CursorPos < (int)m_Text.size()) {
                    int end = nextCharEnd(m_Text, m_CursorPos);
                    m_Text.erase(m_CursorPos, end - m_CursorPos);
                    changed = true;
                }
                break;
            }
            case Key::Backspace: {
                if (m_CursorPos > 0) {
                    int start = prevCharStart(m_Text, m_CursorPos);
                    m_Text.erase(start, m_CursorPos - start);
                    m_CursorPos = start;
                    changed = true;
                }
                break;
            }
        }

        if (changed) {
            RequestRepaint();
            NotifyTextChange();
        } else {
            // 光标移动也需重绘（光标位置变了）
            RequestRepaint();
        }
    }

    void TextInput::OnChar(wchar_t ch) {
        if (m_ReadOnly) return;

        // 只接收可见字符（含中文等非 ASCII）；控制字符已由 OnKeyDown/系统处理
        if (ch >= 32 && ch != 127) {
            std::string utf8 = Utf8FromWide(ch);
            // 按字节偏移插入（m_Text 是 UTF-8）
            m_Text.insert(m_CursorPos, utf8);
            m_CursorPos += (int)utf8.size();
            RequestRepaint();
            NotifyTextChange();
        }
    }

    void TextInput::NotifyTextChange() {
        if (OnTextChange) OnTextChange(m_Text);
    }

}
