#include "Component/ScrollArea.h"

namespace X_Y {

    void ScrollArea::ClampOffset() {
        if (m_ScrollOffset < 0) m_ScrollOffset = 0;
        if (m_Content) {
            int maxOffset = m_Content->GetHeight() - GetHeight();
            if (maxOffset < 0) maxOffset = 0;
            if (m_ScrollOffset > maxOffset) m_ScrollOffset = maxOffset;
        }
    }

    void ScrollArea::SetScrollOffset(int offset) {
        m_ScrollOffset = offset;
        ClampOffset();
        // 状态变了，主动请求所属窗口重绘（不依赖外部 Ticker 顺带刷新）
        RequestRepaint();
    }

    bool ScrollArea::NeedsScrollbar() const {
        if (!m_Content) return false;
        return m_Content->GetHeight() > GetHeight();
    }

    // 滑块矩形（ScrollArea 局部坐标）：右侧 kScrollbarWidth 宽，y/h 为局部值

    void ScrollArea::GetThumbRect(int& outY, int& outH) const {

        int viewH = GetHeight();
        int contentH = m_Content ? m_Content->GetHeight() : 0;

        if (contentH <= viewH) {
            outY = 0;
            outH = viewH;
            return;
        }

        outH = (int)((float)viewH * viewH / contentH);
        if (outH < kMinThumbH) outH = kMinThumbH;

        int scrollRange = contentH - viewH;
        int thumbRange = viewH - outH;
        outY = (int)((float)m_ScrollOffset / scrollRange * thumbRange);
    }

    void ScrollArea::DrawScrollbar(Canvas& canvas) const {
        if (!NeedsScrollbar()) return;

        int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();
        int sbX = x + w - kScrollbarWidth;

        // 轨道
        canvas.FillRect(sbX, y, kScrollbarWidth, h, 0xFF2A2A2A);

        int thumbY, thumbH;
        GetThumbRect(thumbY, thumbH);

        canvas.FillRect(sbX, y + thumbY, kScrollbarWidth, thumbH, 0xFF5A5A5A);
    }

    void ScrollArea::OnPaint(Canvas& canvas) {
        if (!m_Content) return;

        int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();
        int viewW = GetViewWidth();

        // 内容裁到可视区
        canvas.SetClip(x, y, w, h);

        int oldX = m_Content->GetX();
        int oldY = m_Content->GetY();
        m_Content->SetRect(x, y - m_ScrollOffset, viewW, m_Content->GetHeight());
        // 通知内容本次可视范围（供支持可视裁剪的内容如 ListBox 只画可视行）
        m_Content->SetViewport(m_ScrollOffset, h);
        m_Content->OnPaint(canvas);
        m_Content->SetRect(oldX, oldY, viewW, m_Content->GetHeight());

        canvas.ResetClip();

        DrawScrollbar(canvas);
    }

    // ── 滑块交互 ──────────────────────────────────────────────

    void ScrollArea::OnMousePressed(int localX, int localY) {
        if (!NeedsScrollbar()) { m_DraggingThumb = false; return; }

        // 只响应滑块条那一条竖带
        int barX0 = GetWidth() - kScrollbarWidth;
        if (localX < barX0) { m_DraggingThumb = false; return; }

        int thumbY, thumbH;
        GetThumbRect(thumbY, thumbH);

        if (localY >= thumbY && localY < thumbY + thumbH) {
            // 命中滑块 → 开始拖动
            m_DraggingThumb = true;
            m_DragOffsetY = localY - thumbY;
        }
        else {
            // 点击轨道：翻页
            m_DraggingThumb = false;
            int viewH = GetHeight();
            int pageStep = viewH - GetScrollStep();
            if (pageStep < GetScrollStep()) pageStep = GetScrollStep();

            if (localY < thumbY) {
                // 点击滑块上方 → 向上翻页（看更早内容）
                SetScrollOffset(m_ScrollOffset - pageStep);
            }
            else {
                // 点击滑块下方 → 向下翻页（看更晚内容）
                SetScrollOffset(m_ScrollOffset + pageStep);
            }
        }
    }

    void ScrollArea::OnMouseMoved(int localX, int localY) {
        if (!m_DraggingThumb) return;

        int viewH = GetHeight();
        int contentH = m_Content ? m_Content->GetHeight() : 0;
        if (contentH <= viewH) return;

        int thumbH;
        {
            int ty;
            GetThumbRect(ty, thumbH);
        }
        int thumbRange = viewH - thumbH;

        // 新滑块顶 y = 鼠标局部 y - 按下偏移
        int newThumbY = localY - m_DragOffsetY;
        if (newThumbY < 0) newThumbY = 0;
        if (newThumbY > thumbRange) newThumbY = thumbRange;

        int scrollRange = contentH - viewH;
        SetScrollOffset((int)((float)newThumbY / thumbRange * scrollRange));
    }

    void ScrollArea::OnMouseReleased(int localX, int localY) {
        m_DraggingThumb = false;
    }

}
