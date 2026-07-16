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
    }

    void ScrollArea::OnPaint(Canvas& canvas) {
        if (!m_Content) return;

        int x = GetX(), y = GetY(), w = GetWidth(), h = GetHeight();

        canvas.SetClip(x, y, w, h);

        int oldX = m_Content->GetX();
        int oldY = m_Content->GetY();
        m_Content->SetRect(x, y - m_ScrollOffset, w, m_Content->GetHeight());
        m_Content->OnPaint(canvas);
        m_Content->SetRect(oldX, oldY, w, m_Content->GetHeight());

        canvas.ResetClip();
    }

}
