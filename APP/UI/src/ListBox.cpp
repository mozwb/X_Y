#include "UI/include/Component/ListBox.h"

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
        int total = (int)m_Items.size();
        if (total <= 0) return;

        // 可视裁剪：只在 ScrollArea 里（m_ViewHeight 已设置）时生效，
        // 只画屏上可视区 [rowFirst, rowLast) 的行，避免全量 DrawText。

        int rowFirst = 0;
        int rowLast = total;
        if (m_ViewHeight > 0 && m_LineHeight > 0) {
            rowFirst = m_ViewOffset / m_LineHeight;
            rowLast  = (m_ViewOffset + m_ViewHeight) / m_LineHeight + 1;
            if (rowFirst < 0) rowFirst = 0;
            if (rowLast > total) rowLast = total;
            if (rowFirst > rowLast) rowFirst = rowLast;
        }

        for (int i = rowFirst; i < rowLast; i++) {
            int rowY = y + i * m_LineHeight;
            int rowH = m_LineHeight;

            // 行背景色：选中行用高亮，其余用窗口底色（LogViewer 铺的深底）
            uint32_t bgColor = 0xFF1E1E1E;
            if (i == m_SelectedIndex)
                bgColor = 0x00D0E8FF;

            // 组合拳：铺行背景 + 同背景色写字（ClearType 亚像素用真实背景）
            // 背景整行 (x,rowY,w,rowH)；文字起点带缩进 (x+4,rowY+2)

            canvas.FillText(x, rowY, w, rowH, x + 4, rowY + 2,
                m_Items[i].text.c_str(),
                m_Items[i].textColor, bgColor);
        }
    }

    void ListBox::UpdateHeight() {
        SetRect(GetX(), GetY(), GetWidth(),
            (int)m_Items.size() * m_LineHeight);
    }

}
