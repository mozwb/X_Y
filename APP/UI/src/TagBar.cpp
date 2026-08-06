#include "UI/include/Component/TagBar.h"
#include <algorithm>

namespace X_Y {

    void TagBar::AddTag(const std::string& tag) {
        if (tag.empty()) return;
        for (const auto& t : m_Tags)
            if (t == tag) return;
        m_Tags.push_back(tag);
        RequestRepaint();
    }

    void TagBar::RemoveTag(const std::string& tag) {
        auto it = std::find(m_Tags.begin(), m_Tags.end(), tag);
        if (it != m_Tags.end()) {
            m_Tags.erase(it);
            RequestRepaint();
        }
    }

    void TagBar::Clear() {
        m_Tags.clear();
        RequestRepaint();
    }

    // 排版并返回总高：测每个 tag 文字宽，横排 + 自动换行，算出整体矩形并更新自身高。
    // 宿主布局前先 SetRect 设宽再调它量高；OnPaint 也会调它保证绘制一致。
    // 布局：tag = 圆角矩形。右上角留「× 删除按钮」小方块，文字从左侧开始。

    int TagBar::Measure(Canvas& canvas) {
        int contentW = GetWidth();
        if (contentW <= 0 || m_Tags.empty()) {
            m_TagRects.clear();
            m_CloseRects.clear();
            SetRect(GetX(), GetY(), 0, 0);   // 无 tag 时高度压为 0，不占顶部空间
            return 0;
        }

        m_TagRects.clear();
        m_CloseRects.clear();
        m_TagRects.reserve(m_Tags.size());
        m_CloseRects.reserve(m_Tags.size());

        const int btnSize = 12;         // 删除按钮边长
        const int btnPad = 3;           // 按钮距 tag 上/右内边距
        const int textPadLeft = 8;      // 文字左侧内边距
        const int textPadRight = btnSize + btnPad * 3;  // 右侧留按钮区

        int x = 0, y = 0;
        int rowH = m_TagHeight;
        int rows = 1;

        for (size_t i = 0; i < m_Tags.size(); i++) {
            int textW = canvas.MeasureText(m_Tags[i].c_str());
            int tagW = textW + textPadLeft + textPadRight;
            if (tagW < rowH) tagW = rowH;

            // 放不下就换行
            if (x > 0 && x + tagW > contentW) {
                x = 0;
                y += rowH + m_Gap;
                rows++;
            }

            m_TagRects.push_back({ x, y + m_MarginY, tagW, rowH });
            // × 删除按钮：tag 右上角小方块
            m_CloseRects.push_back({ x + tagW - btnSize - btnPad, y + m_MarginY + btnPad, btnSize, btnSize });
            x += tagW + m_Gap;
        }

        int totalH = rows * rowH + (rows - 1) * m_Gap + 2 * m_MarginY;
        SetRect(GetX(), GetY(), GetWidth(), totalH);
        return totalH;
    }

    int TagBar::HitTest(int x, int y) const {
        for (int i = 0; i < (int)m_TagRects.size(); i++) {
            const Rect& r = m_TagRects[i];
            if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
                return i;
        }
        return -1;
    }

    bool TagBar::IsInClose(int index, int x, int y) const {
        if (index < 0 || index >= (int)m_CloseRects.size()) return false;
        const Rect& r = m_CloseRects[index];
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }

    void TagBar::OnPaint(Canvas& canvas) {
        Measure(canvas);

        const int x0 = GetX(), y0 = GetY();

        for (size_t i = 0; i < m_Tags.size(); i++) {
            const Rect& r = m_TagRects[i];
            int rx = x0 + r.x, ry = y0 + r.y;

            bool hover = ((int)i == m_HoverIndex);
            uint32_t bg = hover ? 0xFF2F6F9F : 0xFF3C3C46;
            uint32_t textColor = 0xFFE8E8E8;

            // 圆角 tag 背景
            canvas.FillRoundRect(rx, ry, r.w, r.h, 6, bg);

            // 文字：从左侧开始，垂直居中
            canvas.DrawText(rx + m_PadX_text, ry + (r.h - 14) / 2, m_Tags[i].c_str(), textColor);

            // 右上角 × 删除按钮（圆角小方块 + × 字符）
            const Rect& c = m_CloseRects[i];
            int cx = x0 + c.x, cy = y0 + c.y;
            canvas.FillRoundRect(cx, cy, c.w, c.h, 3, hover ? 0xFF6A7280 : 0xFF555A63);
            canvas.DrawText(cx + (c.w - 10) / 2, cy + (c.h - 12) / 2, L"×", 0xFFEEEEEE);
        }
    }

    void TagBar::OnMouseMoved(int localX, int localY) {
        int idx = HitTest(localX, localY);
        if (idx != m_HoverIndex) {
            m_HoverIndex = idx;
            RequestRepaint();
        }
    }

    void TagBar::OnMousePressed(int localX, int localY) {
        int idx = HitTest(localX, localY);
        if (idx >= 0 && IsInClose(idx, localX, localY)) {
            if (idx < (int)m_Tags.size() && OnTagRemove)
                OnTagRemove(m_Tags[idx]);
        }
    }

    void TagBar::OnMouseReleased(int localX, int localY) {}

}
