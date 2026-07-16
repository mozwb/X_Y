#include "Component/ListBox.h"

namespace X_Y {

    void ListBox::AddItem(const char* text, uint32_t textColor) {
        ListBoxItem item;
        item.text = text;
        item.textColor = textColor;
        m_Items.push_back(item);
        UpdateHeight();
    }

    void ListBox::Clear() {
        m_Items.clear();
        m_SelectedIndex = -1;
        UpdateHeight();
    }

    void ListBox::SetLineHeight(int h) {
        m_LineHeight = h;
        UpdateHeight();
    }

    int ListBox::GetRowFromMouseY(int localY) const {
        return localY / m_LineHeight;
    }

    void ListBox::OnPaint(Canvas& canvas) {
        int x = GetX(), y = GetY(), w = GetWidth();

        for (int i = 0; i < (int)m_Items.size(); i++) {
            int rowY = y + i * m_LineHeight;
            int rowH = m_LineHeight;

            if (i == m_SelectedIndex) {
                canvas.FillRect(x, rowY, w, rowH, 0x00D0E8FF);
            }

            canvas.DrawText(x + 4, rowY + 2,
                m_Items[i].text.c_str(),
                m_Items[i].textColor);
        }
    }

    void ListBox::UpdateHeight() {
        SetRect(GetX(), GetY(), GetWidth(),
            (int)m_Items.size() * m_LineHeight);
    }

}
