#include "../DockLayout/DockLayout.h"

#include <algorithm>

namespace X_Y
{
    namespace
    {
        float ClampT(float v)
        {
            return std::clamp(v, 0.0f, 1.0f);
        }
    }

    DockLayout::~DockLayout()
    {
        m_Docks.clear();
        m_Boundaries.clear();
    }

    void DockLayout::SetActiveSize(int w, int h)
    {
        m_LayoutW = w;
        m_LayoutH = h;
        RecalcLayout();
        RequestRepaint();
    }

    // ── boundary 注册表 ──
    BoundaryId DockLayout::AddBoundary(
        BoundaryOrientation orientation, float line, BoundaryStatus status)
    {
        Boundary boundary;
        boundary.orientation = orientation;
        boundary.status = status;
        boundary.line = ClampT(line);

        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
            if (m_Boundaries[id].removed)
            {
                m_Boundaries[id] = boundary;
                return id;
            }

        m_Boundaries.push_back(boundary);
        return m_Boundaries.size() - 1;
    }

    bool DockLayout::IsValidBoundary(BoundaryId id) const
    {
        return id < m_Boundaries.size() && !m_Boundaries[id].removed;
    }

    const Boundary *DockLayout::GetBoundary(BoundaryId id) const
    {
        return IsValidBoundary(id) ? &m_Boundaries[id] : nullptr;
    }

    int DockLayout::BoundaryPosition(BoundaryId id) const
    {
        if (!IsValidBoundary(id))
            return 0;
        const auto &b = m_Boundaries[id];
        const int extent = b.orientation == BoundaryOrientation::Vertical
                               ? m_LayoutW : m_LayoutH;
        return static_cast<int>(b.line * extent);
    }

    int DockLayout::GetBoundaryPosition(BoundaryId id) const
    {
        return IsValidBoundary(id) ? BoundaryPosition(id) : -1;
    }

    bool DockLayout::SetBoundaryLine(BoundaryId id, float line)
    {
        if (!IsValidBoundary(id))
            return false;
        const auto &b = m_Boundaries[id];
        m_Boundaries[id].line = std::clamp(line, b.min, b.max);
        RecalcLayout();
        RequestRepaint();
        return true;
    }

    bool DockLayout::SetBoundaryColor(BoundaryId id, uint32_t color)
    {
        if (!IsValidBoundary(id))
            return false;
        m_Boundaries[id].color = color;
        RequestRepaint();
        return true;
    }

    bool DockLayout::SetBoundarySize(BoundaryId id, float min, float max)
    {
        if (!IsValidBoundary(id))
            return false;
        m_Boundaries[id].min = ClampT(min);
        m_Boundaries[id].max = ClampT(max);
        return true;
    }

    bool DockLayout::SetBoundaryWidth(BoundaryId id, int width)
    {
        if (!IsValidBoundary(id) || width < 0)
            return false;
        m_Boundaries[id].width = width;
        RecalcLayout();
        RequestRepaint();
        return true;
    }

    bool DockLayout::SetBoundaryRange(BoundaryId id, float start, float end)
    {
        if (!IsValidBoundary(id))
            return false;
        m_Boundaries[id].start = ClampT(start);
        m_Boundaries[id].end = ClampT(end);
        if (m_Boundaries[id].start >= m_Boundaries[id].end)
        {
            m_Boundaries[id].start = 0.0f;
            m_Boundaries[id].end = 1.0f;
        }
        RequestRepaint();
        return true;
    }

    bool DockLayout::MoveBoundary(BoundaryId id, int delta)
    {
        if (!IsValidBoundary(id) ||
            m_Boundaries[id].status != BoundaryStatus::Movable)
            return false;
        const int extent = m_Boundaries[id].orientation == BoundaryOrientation::Vertical
                               ? m_LayoutW : m_LayoutH;
        if (extent <= 0)
            return false;
        return SetBoundaryLine(id, m_Boundaries[id].line +
                                   static_cast<float>(delta) / extent);
    }

    bool DockLayout::RemoveBoundary(BoundaryId id)
    {
        if (!IsValidBoundary(id))
            return false;
        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            auto &slots = dock->GetBoundarySlots();
            if (slots.top == id)    slots.top = InvalidBoundary;
            if (slots.bottom == id) slots.bottom = InvalidBoundary;
            if (slots.left == id)   slots.left = InvalidBoundary;
            if (slots.right == id)  slots.right = InvalidBoundary;
        }
        m_Boundaries[id].removed = true;
        RecalcLayout();
        RequestRepaint();
        return true;
    }

    // ── dock 管理 ──
    void DockLayout::AddDock(Dock *dock)
    {
        if (!dock)
            return;
        if (std::find(m_Docks.begin(), m_Docks.end(), dock) != m_Docks.end())
            return;
        m_Docks.push_back(dock);
        dock->SetDockLayout(this);
        RecalcLayout();
        RequestRepaint();
    }

    bool DockLayout::DockBind(Dock &dock, BoundaryId top, BoundaryId bottom,
                              BoundaryId left, BoundaryId right)
    {
        auto checkSide = [this](BoundaryId id, BoundaryOrientation orient)
        {
            return id == InvalidBoundary || (IsValidBoundary(id) &&
                    m_Boundaries[id].orientation == orient);
        };
        if (!checkSide(top, BoundaryOrientation::Horizontal) ||
            !checkSide(bottom, BoundaryOrientation::Horizontal) ||
            !checkSide(left, BoundaryOrientation::Vertical) ||
            !checkSide(right, BoundaryOrientation::Vertical))
            return false;

        AddDock(&dock);
        auto &slots = dock.GetBoundarySlots();
        slots.top = top;
        slots.bottom = bottom;
        slots.left = left;
        slots.right = right;
        RecalcLayout();
        return true;
    }

    void DockLayout::RemoveDock(Dock *dock)
    {
        m_Docks.erase(std::remove(m_Docks.begin(), m_Docks.end(), dock), m_Docks.end());
        if (dock && dock->GetDockLayout() == this)
            dock->SetDockLayout(nullptr);
        RecalcLayout();
        RequestRepaint();
    }

    // ── 布局重排 ──
    void DockLayout::RecalcLayout()
    {
        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            dock->SetActiveRect(m_LayoutW, m_LayoutH);
        }
        RequestRepaint();
    }

    // ── 命中 + 拖分割线 ──
    BoundaryId DockLayout::HitTestBoundary(int x, int y, int thickness) const
    {
        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            const auto &b = m_Boundaries[id];
            if (b.removed || b.status != BoundaryStatus::Movable)
                continue;
            const int pos = BoundaryPosition(id);
            const bool hit = b.orientation == BoundaryOrientation::Vertical
                                 ? std::abs(x - pos) <= thickness
                                 : std::abs(y - pos) <= thickness;
            if (hit)
                return id;
        }
        return InvalidBoundary;
    }

    bool DockLayout::OnMousePressed(int x, int y)
    {
        // 优先拖分割线：由 Dock 上报命中的边界（共享边同 id，消歧）
        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            const BoundaryId edge = dock->HitTestEdge(x, y);
            if (edge != InvalidBoundary &&
                m_Boundaries[edge].status == BoundaryStatus::Movable)
            {
                m_DraggingBoundary = edge;
                m_LastDragX = x;
                m_LastDragY = y;
                RequestRepaint();
                return true; // 拦截：拖分割线优先
            }
        }
        return false; // 放行：应转发给激活 Panel
    }

    bool DockLayout::OnMouseMoved(int x, int y)
    {
        if (m_DraggingBoundary == InvalidBoundary)
            return false;
        if (m_DraggingBoundary >= m_Boundaries.size() ||
            m_Boundaries[m_DraggingBoundary].removed)
        {
            m_DraggingBoundary = InvalidBoundary;
            return false;
        }
        const bool vertical = m_Boundaries[m_DraggingBoundary].orientation ==
                              BoundaryOrientation::Vertical;
        const int delta = vertical ? (x - m_LastDragX) : (y - m_LastDragY);
        if (delta != 0)
        {
            Boundary &bd = m_Boundaries[m_DraggingBoundary];
            const int extent = bd.orientation == BoundaryOrientation::Vertical
                                   ? m_LayoutW : m_LayoutH;
            if (extent > 0)
                bd.line = std::clamp(bd.line + static_cast<float>(delta) / extent,
                                     bd.min, bd.max);
            RecalcLayout();
        }
        m_LastDragX = x;
        m_LastDragY = y;
        return true;
    }

    void DockLayout::OnMouseReleased(int x, int y)
    {
        (void)x;
        (void)y;
        if (m_DraggingBoundary != InvalidBoundary)
        {
            m_DraggingBoundary = InvalidBoundary;
            RequestRepaint();
        }
    }

    Panel *DockLayout::GetActivePanel() const
    {
        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            Panel *p = dock->GetActivePanel();
            if (p)
                return p;
        }
        return nullptr;
    }

    // ── 绘制 ──
    void DockLayout::OnPaint(Canvas &canvas)
    {
        canvas.Clear(m_BackgroundColor);

        // 各 Dock
        for (Dock *dock : m_Docks)
            if (dock)
                dock->OnPaint(canvas);

        // 分割线
        constexpr int thickness = 2;
        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            const auto &b = m_Boundaries[id];
            if (b.removed)
                continue;
            const int pos = BoundaryPosition(id);
            const uint32_t color = id == m_DraggingBoundary ? b.dragcolor : b.color;
            if (b.orientation == BoundaryOrientation::Vertical)
            {
                const int s = static_cast<int>(b.start * m_LayoutH);
                const int e = static_cast<int>(b.end * m_LayoutH);
                canvas.FillRect(pos - thickness / 2, s, thickness, std::max(1, e - s), color);
            }
            else
            {
                const int s = static_cast<int>(b.start * m_LayoutW);
                const int e = static_cast<int>(b.end * m_LayoutW);
                canvas.FillRect(s, pos - thickness / 2, std::max(1, e - s), thickness, color);
            }
        }
    }

} // namespace X_Y
