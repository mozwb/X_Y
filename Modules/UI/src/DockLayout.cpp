#include "../DockLayout/DockLayout.h"
#include "Movement/MouseMovement.h"
#include "Widget/Application.h"

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

    DockLayout::DockLayout(XWidget *parent)
        : XWidget(parent)
    {
        SetWindowStyle(parent ? WindowStyleFlag::Child | WindowStyleFlag::Visible
                              : WindowStyleFlag::Overlapped | WindowStyleFlag::Resizable);

        // 布局自身 resize → 重排所有受 boundary 约束的 Dock
        Connect(this, MovementType::WindowResize, this,
                [this](const XMovement &)
                { RecalcLayout(); });

        // 方案A：把本布局的全局鼠标钩子注册到 Application。
        // 拖动分割线只在鼠标命中边界热区时拦截，其余放行（内部 dock/container 照常接收）。
        if (auto *app = Application::instance())
        {
            app->SetMouseRedirectHandler(
                [this](const XMovement &e) -> bool
                { return HandleGlobalMouse(e); });
        }
    }

    DockLayout::~DockLayout()
    {
        if (auto *app = Application::instance())
            app->ClearMouseRedirectHandler();
        m_Docks.clear();
        m_Boundaries.clear();
    }

    BoundaryId DockLayout::AddBoundary(
        BoundaryOrientation orientation, float line, BoundaryStatus status)
    {
        Boundary boundary;
        boundary.orientation = orientation;
        boundary.status = status;
        boundary.line = ClampT(line);

        // 优先复用已移除的槽位（墓碑稳定id方案）
        BoundaryId id = FindAvailableBoundarySlot();
        if (id != InvalidBoundary)
        {
            m_Boundaries[id] = boundary;
            return id;
        }

        // 没有可用槽位，新增
        m_Boundaries.push_back(boundary);
        return m_Boundaries.size() - 1;
    }

    bool DockLayout::IsValidBoundary(BoundaryId id) const
    {
        return id < m_Boundaries.size() && !m_Boundaries[id].removed;
    }

    // 左右方向：槽→垂直 boundary 的像素 x，或贴外框（0 = 左缘 / 宽 = 右缘）
    int DockLayout::HorizontalSidePosition(BoundaryId id, bool isRight) const
    {
        if (IsValidBoundary(id))
            return BoundaryPosition(id);
        return isRight ? static_cast<int>(get_width()) : 0;
    }

    // 上下方向：槽→水平 boundary 的像素 y，或贴外框（0 = 上缘 / 高 = 下缘）
    int DockLayout::VerticalSidePosition(BoundaryId id, bool isBottom) const
    {
        if (IsValidBoundary(id))
            return BoundaryPosition(id);
        return isBottom ? static_cast<int>(get_height()) : 0;
    }

    int DockLayout::BoundaryPosition(BoundaryId id) const
    {
        if (!IsValidBoundary(id))
            return 0;
        const auto &boundary = m_Boundaries[id];
        const int extent = boundary.orientation == BoundaryOrientation::Vertical
                               ? static_cast<int>(get_width())
                               : static_cast<int>(get_height());
        return static_cast<int>(boundary.line * extent);
    }

    bool DockLayout::SetBoundaryLine(BoundaryId id, float line)
    {
        if (!IsValidBoundary(id))
            return false;
        const auto &boundary = m_Boundaries[id];
        m_Boundaries[id].line = std::clamp(line, boundary.min, boundary.max);
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
                               ? static_cast<int>(get_width())
                               : static_cast<int>(get_height());
        if (extent <= 0)
            return false;
        return SetBoundaryLine(id, m_Boundaries[id].line +
                                       static_cast<float>(delta) / extent);
    }

    bool DockLayout::RemoveBoundary(BoundaryId id)
    {
        if (!IsValidBoundary(id))
            return false;

        // 先清空所有 Dock 槽里指向它的引用（恢复为 InvalidBoundary=贴外框）
        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            auto &slots = dock->GetBoundarySlots();
            if (slots.top == id)
                slots.top = InvalidBoundary;
            if (slots.bottom == id)
                slots.bottom = InvalidBoundary;
            if (slots.left == id)
                slots.left = InvalidBoundary;
            if (slots.right == id)
                slots.right = InvalidBoundary;
        }

        // 墓碑稳定id方案：标记为removed而非删除，保持id稳定
        m_Boundaries[id].removed = true;
        RecalcLayout();
        RequestRepaint();
        return true;
    }

    const Boundary *DockLayout::GetBoundary(BoundaryId id) const
    {
        return IsValidBoundary(id) ? &m_Boundaries[id] : nullptr;
    }

    int DockLayout::GetBoundaryPosition(BoundaryId id) const
    {
        return IsValidBoundary(id) ? BoundaryPosition(id) : -1;
    }

    void DockLayout::AddDock(Dock *dock)
    {
        if (!dock)
            return;
        const auto it = std::find_if(m_Docks.begin(), m_Docks.end(),
                                     [dock](Dock *d)
                                     { return d == dock; });
        if (it != m_Docks.end())
            return;

        m_Docks.push_back(dock);
        // Dock 关联本布局，以便它把自身变化回传（如内容变化触发重排）
        dock->SetDockLayout(this);
        if (GetNativeHandle())
            dock->SetParentHwnd(GetNativeHandle());
        RecalcLayout();
    }

    bool DockLayout::DockBind(Dock &dock, BoundaryId top, BoundaryId bottom,
                              BoundaryId left, BoundaryId right)
    {
        // 校验：水平边界必须 Horizontal，垂直边界必须 Vertical；InvalidBoundary 放行（贴外框）
        auto checkSide = [this](BoundaryId id, BoundaryOrientation orient)
        {
            return id == InvalidBoundary || IsValidBoundary(id) &&
                                                m_Boundaries[id].orientation == orient;
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
        m_Docks.erase(std::remove_if(m_Docks.begin(), m_Docks.end(),
                                     [dock](Dock *d)
                                     { return d == dock; }),
                      m_Docks.end());
        if (dock && dock->GetDockLayout() == this)
            dock->SetDockLayout(nullptr);
        RecalcLayout();
    }

    void DockLayout::SetBackgroundColor(uint32_t color)
    {
        m_BackgroundColor = color;
        RequestRepaint();
    }

    void DockLayout::OnWindowResize()
    {
        RecalcLayout();
    }

    void DockLayout::RecalcLayout()
    {
        // 缝隙：每条被真实 Boundary 约束的 dock 侧往里让该 boundary.width 的一半，
        // 在 boundary 两侧露出缝隙（总宽=boundary.width），分割线落在缝里可见。
        // 外框侧(InvalidBoundary)不偏移。width 可按"宗"分别 SetBoundaryWidth。
        auto halfGap = [this](BoundaryId id) -> int
        {
            return IsValidBoundary(id) ? m_Boundaries[id].width / 2 : 0;
        };

        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            const auto &slots = dock->GetBoundarySlots();

            const int left = HorizontalSidePosition(slots.left, /*isRight*/ false) +
                             halfGap(slots.left);
            const int right = HorizontalSidePosition(slots.right, /*isRight*/ true) -
                              halfGap(slots.right);
            const int top = VerticalSidePosition(slots.top, /*isBottom*/ false) +
                            halfGap(slots.top);
            const int bottom = VerticalSidePosition(slots.bottom, /*isBottom*/ true) -
                               halfGap(slots.bottom);
            if (right < left || bottom < top)
                continue;

            const int dockW = right - left;
            const int dockH = bottom - top;
            if (dockW <= 0 || dockH <= 0)
                continue;

            // 收敛保护：若 Dock 已是目标尺寸则不再 MoveAndResize。
            // 避免"同尺寸反复 move → 触发 WM_SIZE → 再 RecalcLayout"的 resize 风暴/死循环。
            if ((int)dock->get_width() == dockW && (int)dock->get_height() == dockH)
                continue;

            dock->MoveAndResize(left, top, dockW, dockH);
            dock->RecalcLayout();
        }
    }

    BoundaryId DockLayout::HitTestBoundary(int x, int y, int thickness) const
    {
        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            const auto &boundary = m_Boundaries[id];
            if (boundary.removed || boundary.status != BoundaryStatus::Movable)
                continue;
            const int position = BoundaryPosition(id);
            const bool hit = boundary.orientation == BoundaryOrientation::Vertical
                                 ? std::abs(x - position) <= thickness
                                 : std::abs(y - position) <= thickness;
            if (hit)
                return id;
        }
        return InvalidBoundary;
    }

    void DockLayout::BeginDragBoundary(BoundaryId id)
    {
        if (!IsValidBoundary(id) ||
            m_Boundaries[id].status != BoundaryStatus::Movable)
            return;
        m_DraggingBoundary = id;
        RequestRepaint(); // 异步重绘；拖动态线用 boundary.dragcolor 绘制
    }

    void DockLayout::EndDragBoundary()
    {
        if (m_DraggingBoundary != InvalidBoundary)
        {
            // 检查正在拖动的boundary是否已被移除（理论上不应该发生，但防御性检查）
            if (m_DraggingBoundary < m_Boundaries.size() && !m_Boundaries[m_DraggingBoundary].removed)
            {
                m_DraggingBoundary = InvalidBoundary;
                RequestRepaint(); // 异步重绘；恢复普通 color
            }
        }
    }

    bool DockLayout::HandleGlobalMouse(const XMovement &event)
    {
        // 只关心鼠标三态
        const bool isPress =
            dynamic_cast<const MouseButtonPressed *>(&event) != nullptr;
        const bool isMove =
            dynamic_cast<const MouseMoved *>(&event) != nullptr;
        const bool isRelease =
            dynamic_cast<const MouseButtonReleased *>(&event) != nullptr;
        if (!isPress && !isMove && !isRelease)
            return false;

        // 屏幕坐标 → 布局逻辑客户区坐标
        int sx = 0, sy = 0;
        GetMouseScreenPos(sx, sy);
        ScreenToClient(sx, sy);

        if (isRelease)
        {
            if (m_DraggingBoundary != InvalidBoundary)
            {
                EndDragBoundary();
                ReleaseMouseCapture();
                return true;
            }
            return false;
        }

        if (isMove)
        {
            // 拖动中：按边界方向取增量并移动
            if (m_DraggingBoundary == InvalidBoundary ||
                m_DraggingBoundary >= m_Boundaries.size() ||
                m_Boundaries[m_DraggingBoundary].removed)
                return false;
            const bool vertical = m_Boundaries[m_DraggingBoundary].orientation ==
                                  BoundaryOrientation::Vertical;
            const int delta = vertical ? (sx - m_LastDragMouseX)
                                       : (sy - m_LastDragMouseY);
            if (delta != 0)
            {
                // 直接改 line + 重排 dock（不走 MoveBoundary 的整窗异步 RequestRepaint），
                // 用常驻画布局部上屏，保证线跟手丝滑。
                Boundary &bd = m_Boundaries[m_DraggingBoundary];
                // 改 line 前先记旧位置：上屏区域要并入它，否则屏幕上老线像素
                // 没人覆盖 → 拖动残影（扩大方向必现，画布整幅重画清不掉屏幕）。
                const int oldPos = BoundaryPosition(m_DraggingBoundary);
                const int extent = bd.orientation == BoundaryOrientation::Vertical
                                       ? static_cast<int>(get_width())
                                       : static_cast<int>(get_height());
                if (extent > 0)
                    bd.line = std::clamp(bd.line + static_cast<float>(delta) / extent,
                                         bd.min, bd.max);
                RecalcLayout();              // 先重排 dock
                RedrawBoundaryLines(oldPos); // 再局部上屏（当前窄带 ∪ 旧位置窄带）
            }

            m_LastDragMouseX = sx;
            m_LastDragMouseY = sy;
            return true;
        }

        // 按下：找命中边界。由每个 Dock 自己上报"是哪条边"(共享边界同 id，天然消歧)。
        if (isPress)
        {
            for (Dock *dock : m_Docks)
            {
                if (!dock)
                    continue;
                const BoundaryId edge = dock->HitTestEdge(sx, sy);
                if (edge != InvalidBoundary &&
                    m_Boundaries[edge].status == BoundaryStatus::Movable)
                {
                    BeginDragBoundary(edge);
                    m_LastDragMouseX = sx;
                    m_LastDragMouseY = sy;
                    CaptureMouse();
                    return true; // 拦截：分割线拖动优先，不让内部节点收到本次按下
                }
            }
            return false; // 未命中边界 → 放行，内部容器/组件正常处理
        }

        return false;
    }

    void DockLayout::OnPaint(Canvas *canvas)
    {
        if (canvas)
            DrawLayout(*canvas);
    }

    // 绘制布局背景 + 所有边界分割线。
    // 抽取成独立方法：OnPaint(WM_PAINT 异步) 与拖动即时重绘(同步) 共用，
    // 保证拖动时线能跟手、不被 dock 遮蔽导致的异步滞后掩盖。
    void DockLayout::DrawLayout(Canvas &canvas)
    {
        constexpr int boundaryThickness = 2;

        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());
        canvas.Clear(m_BackgroundColor);

        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            const auto &boundary = m_Boundaries[id];
            if (boundary.removed)
                continue;

            const int position = BoundaryPosition(id);
            // 拖中使用 dragcolor，否则 color。⚠️ 都要带不透明 alpha(0xFF 高位)，
            // 否则 FillRect 会把它当 ARGB 全透明 → 线消失（历史根因：draggingColor=0xFFFFFF 透明）。
            const uint32_t color = id == m_DraggingBoundary
                                       ? boundary.dragcolor
                                       : boundary.color;

            if (boundary.orientation == BoundaryOrientation::Vertical)
            {
                // 垂直线：x = position，跨度沿 y（[start,end]×布局高）
                const int s = static_cast<int>(boundary.start * height);
                const int e = static_cast<int>(boundary.end * height);
                canvas.FillRect(position - boundaryThickness / 2, s,
                                boundaryThickness, std::max(1, e - s), color);
            }
            else
            {
                // 水平线：y = position，跨度沿 x（[start,end]×布局宽）
                const int s = static_cast<int>(boundary.start * width);
                const int e = static_cast<int>(boundary.end * width);
                canvas.FillRect(s, position - boundaryThickness / 2,
                                std::max(1, e - s), boundaryThickness, color);
            }
        }
    }

    // 计算所有分割线窄带的包围盒（逻辑像素）。有可见边界返回 true。
    bool DockLayout::BoundaryRectsBounds(int &x, int &y, int &w, int &h) const
    {
        constexpr int thickness = 2;
        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());

        int minX = width, minY = height, maxX = 0, maxY = 0;
        bool any = false;
        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            const auto &b = m_Boundaries[id];
            if (b.removed)
                continue;

            const int pos = BoundaryPosition(id);
            if (b.orientation == BoundaryOrientation::Vertical)
            {
                const int s = static_cast<int>(b.start * height);
                const int e = static_cast<int>(b.end * height);
                const int rx = pos - thickness / 2, rw = thickness;
                const int ry = s, rh = std::max(1, e - s);
                minX = std::min(minX, rx);
                maxX = std::max(maxX, rx + rw);
                minY = std::min(minY, ry);
                maxY = std::max(maxY, ry + rh);
            }
            else
            {
                const int s = static_cast<int>(b.start * width);
                const int e = static_cast<int>(b.end * width);
                const int rx = s, rw = std::max(1, e - s);
                const int ry = pos - thickness / 2, rh = thickness;
                minX = std::min(minX, rx);
                maxX = std::max(maxX, rx + rw);
                minY = std::min(minY, ry);
                maxY = std::max(maxY, ry + rh);
            }
            any = true;
        }
        if (!any)
            return false;
        x = minX;
        y = minY;
        w = maxX - minX;
        h = maxY - minY;
        return (w > 0 && h > 0);
    }

    // 拖动即时局部刷新：整窗画到常驻 GetCanvas（复用不 new），再 FlushArea 只上屏
    // 分割线窄带的包围盒。比每 move 走 WM_PAINT 整窗 new+BitBlt 快，丝滑。
    // includeOldPos = 被拖边界旧位置时，把旧位置窄带并入包围盒（画布里旧位置已
    // 是背景色，拷上屏即清掉屏幕残影；越界由 FlushRect 内部 clamp 兜底）。
    void DockLayout::RedrawBoundaryLines(int includeOldPos)
    {
        if (!GetNativeHandle())
            return;
        Canvas &c = GetCanvas();
        DrawLayout(c);
        int bx, by, bw, bh;
        if (!BoundaryRectsBounds(bx, by, bw, bh))
            return;

        if (includeOldPos >= 0 && m_DraggingBoundary != InvalidBoundary &&
            m_DraggingBoundary < m_Boundaries.size() && !m_Boundaries[m_DraggingBoundary].removed)
        {
            // 旧位置窄带：与 BoundaryRectsBounds 对单条边界的算法一致（thickness=2）
            constexpr int thickness = 2;
            const auto &bd = m_Boundaries[m_DraggingBoundary];
            const int width = static_cast<int>(get_width());
            const int height = static_cast<int>(get_height());
            int rx, ry, rw, rh;
            if (bd.orientation == BoundaryOrientation::Vertical)
            {
                rx = includeOldPos - thickness / 2;
                rw = thickness;
                ry = static_cast<int>(bd.start * height);
                rh = std::max(1, static_cast<int>(bd.end * height) - ry);
            }
            else
            {
                rx = static_cast<int>(bd.start * width);
                rw = std::max(1, static_cast<int>(bd.end * width) - rx);
                ry = includeOldPos - thickness / 2;
                rh = thickness;
            }
            // 并入包围盒（旧带可能在当前包围盒之外）
            const int ex = std::max(bx + bw, rx + rw);
            const int ey = std::max(by + bh, ry + rh);
            bx = std::min(bx, rx);
            by = std::min(by, ry);
            bw = ex - bx;
            bh = ey - by;
        }

        FlushArea(bx, by, bw, bh);
    }

    // 查找可用的boundary槽位（优先复用已移除的墓碑，没有则新增）
    BoundaryId DockLayout::FindAvailableBoundarySlot()
    {
        // 优先查找已移除的槽位
        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            if (m_Boundaries[id].removed)
                return id;
        }

        // 没有可复用的墓碑槽位，返回InvalidBoundary表示需要新增
        return InvalidBoundary;
    }

}
