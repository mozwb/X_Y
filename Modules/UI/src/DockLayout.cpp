#include "../DockLayout/DockLayout.h"
#include "../UiCore/UINode.h"

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
        // ⚠️ 析构顺序：
        //   1. 先让所有 Dock 断掉指向本布局的回指（避免 Dock 析构期间回调本布局）
        //   2. 再释放 Dock（unique_ptr 容器 clear 即可）
        for (Dock *dock : m_Docks)
        {
            if (dock && dock->GetDockLayout() == this)
                dock->SetDockLayout(nullptr);
        }
        m_Docks.clear();

        // Dock 全归本布局 —— 不论无宗（初始区域）还是有父（Split 切出）。
        m_OwnedDocks.clear();

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
                               ? m_LayoutW
                               : m_LayoutH;
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

        const Boundary &boundary = m_Boundaries[id];
        line = std::clamp(line, boundary.min, boundary.max);
        for (Dock *dock : m_Docks)
        {
            if (dock && !dock->IsBoundaryPositionValid(id, line))
                return false;
        }

        m_Boundaries[id].line = line;
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

    bool DockLayout::SetBoundaryRange(BoundaryId id, float min, float max)
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

    bool DockLayout::SetBoundarySize(BoundaryId id, BoundaryId startBoundary,
                                     BoundaryId endBoundary)
    {
        if (!IsValidBoundary(id))
            return false;
        if (startBoundary != InvalidBoundary && !IsValidBoundary(startBoundary))
            return false;
        if (endBoundary != InvalidBoundary && !IsValidBoundary(endBoundary))
            return false;

        m_Boundaries[id].startBoundary = startBoundary;
        m_Boundaries[id].endBoundary = endBoundary;
        RequestRepaint();
        return true;
    }

    bool DockLayout::GetBoundarySize(BoundaryId id, float &start, float &end) const
    {
        if (!IsValidBoundary(id))
            return false;

        const Boundary &boundary = m_Boundaries[id];
        const bool vertical = boundary.orientation == BoundaryOrientation::Vertical;
        const int extent = vertical ? m_LayoutH : m_LayoutW;
        auto resolve = [this, extent](BoundaryId rangeBoundary, float fallback) -> float
        {
            if (rangeBoundary == InvalidBoundary)
                return fallback;
            const Boundary *boundary = GetBoundary(rangeBoundary);
            if (!boundary)
                return fallback;
            const int boundaryExtent = boundary->orientation == BoundaryOrientation::Vertical
                                           ? m_LayoutW
                                           : m_LayoutH;
            if (boundaryExtent <= 0 || extent <= 0)
                return fallback;
            return static_cast<float>(BoundaryPosition(rangeBoundary)) / extent;
        };

        start = ClampT(resolve(boundary.startBoundary, 0.0f));
        end = ClampT(resolve(boundary.endBoundary, 1.0f));
        if (start > end)
            std::swap(start, end);
        return true;
    }

    bool DockLayout::MoveBoundary(BoundaryId id, int delta)
    {
        if (!IsValidBoundary(id) ||
            m_Boundaries[id].status != BoundaryStatus::Movable)
            return false;
        const int extent = m_Boundaries[id].orientation == BoundaryOrientation::Vertical
                               ? m_LayoutW
                               : m_LayoutH;
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
            if (slots.top == id)
                slots.top = InvalidBoundary;
            if (slots.bottom == id)
                slots.bottom = InvalidBoundary;
            if (slots.left == id)
                slots.left = InvalidBoundary;
            if (slots.right == id)
                slots.right = InvalidBoundary;
        }
        m_Boundaries[id].removed = true;
        RecalcLayout();
        RequestRepaint();
        return true;
    }

    // ── dock 管理 ──
    //
    // ★ 所有权：AddDock 接管 dock（存进 unique_ptr），调用方不要再 delete。
    //   无宗 Dock（初始区域划分）与有父 Dock（Split 切出）走同一个入口 ——
    //   它们的区别只在"相对位置固定与否 / 归还方式"，不在归属。
    void DockLayout::AddDock(Dock *dock)
    {
        if (!dock)
            return;
        if (std::find(m_Docks.begin(), m_Docks.end(), dock) != m_Docks.end())
            return;

        m_OwnedDocks.emplace_back(dock); // 登记所有权
        m_Docks.push_back(dock);         // 登记借用视图

        dock->SetDockLayout(this);
        dock->SetHostRepaint(m_HostRepaint);
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

        AddDock(&dock); // 所有权归本布局（引用传入只是书写方便）
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
        if (!dock)
            return;

        // 先摘掉借用指针，防止拖拽/悬停状态指向即将销毁的 Dock
        if (m_MouseCaptureDock == dock)
            m_MouseCaptureDock = nullptr;
        if (m_FileDropPanel && dock->ContainsPanel(m_FileDropPanel))
            m_FileDropPanel = nullptr;

        m_Docks.erase(std::remove(m_Docks.begin(), m_Docks.end(), dock), m_Docks.end());

        if (dock->GetDockLayout() == this)
            dock->SetDockLayout(nullptr);

        // 释放（unique_ptr 析构 → delete）。Dock 归本布局，一律释放。
        auto it = std::find_if(m_OwnedDocks.begin(), m_OwnedDocks.end(),
                               [dock](const std::unique_ptr<Dock> &p)
                               { return p.get() == dock; });
        if (it != m_OwnedDocks.end())
            m_OwnedDocks.erase(it);

        RecalcLayout();
        RequestRepaint();
    }

    // 按落点收容一个 Panel。
    // 落点命中哪个 Dock 就交给它；落空返回 nullptr（此时 panel 已被释放，
    // 因为没有 Dock 接管它 —— 调用方若想保住它，应该自己持有 unique_ptr）。
    //
    // ℹ️ 早期这里有个 TakePanel(panel, title)：语义是"找第一个【有】激活面板的
    //    Dock 塞进去"，方向正好相反（该找的是**能收容**的 Dock），且会 new 裸 Dock
    //    （没有 tab 栏）却无人释放。它从未被调用，已删除 —— 按落点收容走本函数。
    Panel *DockLayout::AddPanelAt(std::unique_ptr<Panel> panel, int x, int y,
                                  const std::string &title)
    {
        if (!panel)
            return nullptr;

        // 复用 HitTestDock：与绘制 z 序 / 事件命中同一套顺序（逆序 = 上层优先）
        Dock *dock = HitTestDock(x, y);
        if (!dock || !dock->CanAddPanel())
            return nullptr; // panel 随参数析构被释放

        return dock->AddPanel(std::move(panel), title);
    }

    void DockLayout::RouteFileDragEnter(const std::vector<XPath> &files, int x, int y)
    {
        RouteFileDragOver(files, x, y);
    }

    void DockLayout::RouteFileDragOver(const std::vector<XPath> &files, int x, int y)
    {
        Panel *target = nullptr;
        for (Dock *dock : m_Docks)
        {
            if (!dock || x < dock->GetX() || x >= dock->GetX() + dock->GetWidth() ||
                y < dock->GetY() || y >= dock->GetY() + dock->GetHeight())
                continue;

            const int localX = x - dock->GetX();
            const int localY = y - dock->GetY();
            target = dock->HitTestPanel(localX, localY);
            if (target)
            {
                // Panel 的矩形是【Dock 局部坐标】，转成布局绝对要加上 Dock 的位置。
                const int panelX = dock->GetX() + target->GetX();
                const int panelY = dock->GetY() + target->GetY();
                if (target != m_FileDropPanel)
                {
                    if (m_FileDropPanel)
                        m_FileDropPanel->OnFileDragLeave();
                    m_FileDropPanel = target;
                    target->OnFileDragEnter(files, x - panelX, y - panelY);
                }
                else
                {
                    target->OnFileDragOver(files, x - panelX, y - panelY);
                }
                return;
            }
        }

        if (m_FileDropPanel)
        {
            m_FileDropPanel->OnFileDragLeave();
            m_FileDropPanel = nullptr;
        }
    }

    void DockLayout::RouteFileDragLeave()
    {
        if (m_FileDropPanel)
        {
            m_FileDropPanel->OnFileDragLeave();
            m_FileDropPanel = nullptr;
        }
    }

    void DockLayout::RouteFileDrop(const std::vector<XPath> &files, int x, int y)
    {
        RouteFileDragOver(files, x, y);
        if (m_FileDropPanel)
        {
            Panel *target = m_FileDropPanel;
            // 需要 Panel 的【布局绝对】原点：Panel 自身存的是 Dock 局部坐标，
            // 所以先找到它所在的 Dock，再加上 Dock 位置。
            int originX = target->GetX();
            int originY = target->GetY();
            for (Dock *dock : m_Docks)
            {
                if (!dock || !dock->ContainsPanel(target))
                    continue;
                originX += dock->GetX();
                originY += dock->GetY();
                break;
            }
            target->OnFileDrop(files, x - originX, y - originY);
            target->OnFileDragLeave();
            m_FileDropPanel = nullptr;
        }
    }

    // ── 布局重排 ──
    // ⚠️ 尺寸为 0 时直接返回：此刻还没有真实尺寸（窗口未建/未 resize），
    //    继续算会把所有 Dock 压成 0×0，连 tab 栏都被裁掉。
    //    真正的尺寸会在 SetActiveSize（壳 resize / SetDockLayout）时喂进来。
    void DockLayout::RecalcLayout()
    {
        if (m_LayoutW <= 0 || m_LayoutH <= 0)
            return;

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
            float start = 0.0f;
            float end = 1.0f;
            GetBoundarySize(id, start, end);
            const bool hit = b.orientation == BoundaryOrientation::Vertical
                                 ? (std::abs(x - pos) <= thickness &&
                                    y >= start * m_LayoutH && y <= end * m_LayoutH)
                                 : (std::abs(y - pos) <= thickness &&
                                    x >= start * m_LayoutW && x <= end * m_LayoutW);
            if (hit)
                return id;
        }
        return InvalidBoundary;
    }

    void DockLayout::RouteInput(UIInputEvent &e)
    {
        auto *me = dynamic_cast<UIMouseEvent *>(&e);

        // 鼠标事件：优先拖分割线（语义化为 Press/Move/Release 状态机）
        if (me)
        {
            if (me->action == MouseAction::Press)
            {
                m_MouseCaptureDock = nullptr;
                // 按下：命中分割线 → 进入拖拽（拦截，不下传）
                // 分割线优先于所有 Dock（它画在最上层，也该最先被命中）。
                for (Dock *dock : m_Docks)
                {
                    if (!dock)
                        continue;
                    const BoundaryId edge = dock->HitTestEdge(e.x, e.y);
                    if (edge != InvalidBoundary &&
                        m_Boundaries[edge].status == BoundaryStatus::Movable)
                    {
                        m_DraggingBoundary = edge;
                        m_LastDragX = e.x;
                        m_LastDragY = e.y;
                        RequestRepaint();
                        return;
                    }
                }
                m_DraggingBoundary = InvalidBoundary;

                // 未命中分割线 → 记下命中的 Dock，从按下到抬起都用它。
                // ★ 逆序遍历：与 OnPaint 的绘制 z 序对齐（后画的在上层，命中优先）。
                //   正序会让"画在最上面"的 Dock 反而最难命中（右侧/底部 Dock 吃不到事件）。
                m_MouseCaptureDock = HitTestDock(e.x, e.y);
            }
            else if (me->action == MouseAction::Move)
            {
                // 拖拽中：拖动分割线
                if (m_DraggingBoundary != InvalidBoundary)
                {
                    if (m_DraggingBoundary >= m_Boundaries.size() ||
                        m_Boundaries[m_DraggingBoundary].removed)
                    {
                        m_DraggingBoundary = InvalidBoundary;
                    }
                    else
                    {
                        const bool vertical = m_Boundaries[m_DraggingBoundary].orientation ==
                                              BoundaryOrientation::Vertical;
                        const int delta = vertical ? (e.x - m_LastDragX)
                                                   : (e.y - m_LastDragY);
                        if (delta != 0)
                        {
                            Boundary &bd = m_Boundaries[m_DraggingBoundary];
                            const int extent = bd.orientation == BoundaryOrientation::Vertical
                                                   ? m_LayoutW
                                                   : m_LayoutH;
                            if (extent > 0)
                                SetBoundaryLine(m_DraggingBoundary,
                                                bd.line + static_cast<float>(delta) / extent);
                        }
                        m_LastDragX = e.x;
                        m_LastDragY = e.y;
                    }
                    return; // 拖拽中拦截，不下传
                }
            }
            else if (me->action == MouseAction::Release)
            {
                if (m_DraggingBoundary != InvalidBoundary)
                {
                    m_DraggingBoundary = InvalidBoundary;
                    RequestRepaint();
                    return;
                }
            }
        }

        // 键盘事件：无坐标意义，下传给"有激活面板"的 dock（其 Panel 内部走焦点）
        if (dynamic_cast<UIKeyEvent *>(&e))
        {
            for (Dock *dock : m_Docks)
            {
                if (!dock || !dock->GetActivePanel())
                    continue;
                dock->RouteInput(e);
                return;
            }
            return;
        }

        // 非拖拽：下传给命中的 Dock（平移成 Dock 局部坐标）
        // 优先用按下时锁定的 Dock（拖动中指针可能移出该 Dock，仍应继续喂给它）；
        // 没有锁定（如按下发生在布局外）则现算一次命中。
        Dock *targetDock = m_MouseCaptureDock;
        if (!targetDock)
            targetDock = HitTestDock(e.x, e.y);

        if (me && me->action == MouseAction::Release)
            m_MouseCaptureDock = nullptr;

        if (targetDock)
        {
            // 下钻到 Dock：用共用原语（View(Dock).self 在布局坐标系里）
            const UINodeView dv = View(*targetDock);
            ToLocal(dv, e.x, e.y);
            targetDock->RouteInput(e);
            ToParent(dv, e.x, e.y); // 还原，供上层保持视角
            return;
        }
    }

    // 命中哪个 Dock（入参为布局绝对坐标）。
    // ★ 逆序遍历：与 OnPaint 的绘制 z 序一致 —— 后画的 Dock 在上层，应优先命中。
    // ★ 判定用 self（【整个 Dock 矩形】），不能用 Hits()/content：
    //   content 是"面板内容区"（避开 tab 栏），若拿它找 Dock，
    //   鼠标按在 tab 栏上时就找不到任何 Dock → 事件下不去 → tab 拖不动。
    //   「命中哪个 Dock」和「命中 Dock 内的面板内容」是两件事：
    //     外层用 self（Dock 全矩形，含 tab 栏）
    //     内层用 content（Dock::HitTestPanel）
    Dock *DockLayout::HitTestDock(int x, int y) const
    {
        for (auto it = m_Docks.rbegin(); it != m_Docks.rend(); ++it)
        {
            Dock *dock = *it;
            if (dock && View(*dock).self.Contains(x, y))
                return dock;
        }
        return nullptr;
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
    // 坐标约定（写法乙）：DockLayout 画的是布局绝对坐标；遍历到某个 Dock 时
    // 压入它的位置，于是 Dock 内部（tab 栏、Panel、组件）全程按 (0,0) 起画。
    // 与输入链对称：
    //   绘制: 绝对 ─PushOrigin(dock.X/Y)→ Dock局部 ─PushOrigin(panelX/Y)→ Panel局部
    //   输入: 绝对 ─减 dock.GetX/Y→        Dock局部 ─减 panelX/Y→         Panel局部
    void DockLayout::OnPaint(Canvas &canvas)
    {
        canvas.Clear(m_BackgroundColor);

        // 各 Dock
        // 每个 Dock 前压 origin + 设自己的裁剪区：Dock 内容（含 Panel 溢出）
        // 不许画到本 Dock 矩形之外，避免相邻 Dock 互相覆盖。
        // 几何走 View(*dock)（与命中共用同一份描述）。
        for (Dock *dock : m_Docks)
        {
            if (!dock)
                continue;
            const UINodeView dv = View(*dock);
            canvas.PushOrigin(dv.self.x, dv.self.y);
            canvas.SetClip(0, 0, dv.self.w, dv.self.h);
            dock->OnPaint(canvas);
            canvas.ResetClip();
            canvas.PopOrigin();
        }

        // 分割线
        // ★ 线宽必须用 boundary 自己的 width（= 缝隙总宽），不能用写死的 thickness：
        //   Dock::RecalcRect 是让出 width/2 的缝，若这里只画 2px，剩下的缝就是"真空"，
        //   露出背景色，看着像线被 Dock 压扁。让多少缝就画多宽 —— 两边同一个值。
        for (BoundaryId id = 0; id < m_Boundaries.size(); ++id)
        {
            const auto &b = m_Boundaries[id];
            if (b.removed)
                continue;
            const int pos = BoundaryPosition(id);
            const int lineW = std::max(1, b.width); // 与 Dock 让缝同源
            float start = 0.0f;
            float end = 1.0f;
            GetBoundarySize(id, start, end);
            const uint32_t color = id == m_DraggingBoundary ? b.dragcolor : b.color;
            if (b.orientation == BoundaryOrientation::Vertical)
            {
                const int s = static_cast<int>(start * m_LayoutH);
                const int e = static_cast<int>(end * m_LayoutH);
                canvas.FillRect(pos - lineW / 2, s, lineW, std::max(1, e - s), color);
            }
            else
            {
                const int s = static_cast<int>(start * m_LayoutW);
                const int e = static_cast<int>(end * m_LayoutW);
                canvas.FillRect(s, pos - lineW / 2, std::max(1, e - s), lineW, color);
            }
        }
    }

} // namespace X_Y
