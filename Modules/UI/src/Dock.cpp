#include "../dock/Dock.h"
#include "../DockLayout/DockLayout.h"

#include <algorithm>

namespace X_Y
{

    Dock::Dock(XWidget *parent)
        : XWidget(parent)
    {
        SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible |
                       WindowStyleFlag::ClipChildren | WindowStyleFlag::ClipSiblings);
        if (parent && parent->GetNativeHandle())
            SetParentHwnd(parent->GetNativeHandle());
    }

    Dock::~Dock()
    {
        // 基类析构，子类负责清理自己的资源
    }

    Container *Dock::AddContainer(Container *container, const std::string &title)
    {
        if (!container)
            return nullptr;

        // 检查是否超过最大承载数
        if (m_Containers.size() >= static_cast<size_t>(m_MaxContainers))
            return nullptr;

        // 检查是否已存在
        for (const auto &entry : m_Containers)
        {
            if (entry.container == container)
                return container;
        }

        ContainerEntry entry;
        entry.container = container;
        entry.title = title;
        m_Containers.push_back(std::move(entry));

        // 激活新添加的容器
        m_ActiveIndex = (int)m_Containers.size() - 1;

        // 通知子类
        OnContainerAdded(container);

        return container;
    }

    bool Dock::RemoveContainer(Container *container)
    {
        for (auto it = m_Containers.begin(); it != m_Containers.end(); ++it)
        {
            if (it->container == container)
            {
                // 隐藏被移除的容器
                if (container->GetNativeHandle())
                    container->show(ShowCmd::Hide);

                // 通知子类
                OnContainerRemoved(container);

                m_Containers.erase(it);

                // 若移除的是激活项，退到最后一个可用项
                if (m_ActiveIndex >= (int)m_Containers.size())
                    m_ActiveIndex = (int)m_Containers.size() - 1;

                // 如果还有容器，激活最后一个
                if (!m_Containers.empty() && m_ActiveIndex < 0)
                    m_ActiveIndex = 0;

                // 如果变空，自动合并
                if (m_Containers.empty())
                {
                    Merge();
                }
                else if (m_ActiveIndex >= 0)
                {
                    ActivateContainer(m_ActiveIndex);
                }

                return true;
            }
        }
        return false;
    }

    void Dock::ActivateContainer(int idx)
    {
        if (idx < 0 || idx >= (int)m_Containers.size())
            return;

        m_ActiveIndex = idx;
        ShowActiveContainer();
    }

    void Dock::ActivateContainer(Container *container)
    {
        for (int i = 0; i < (int)m_Containers.size(); ++i)
        {
            if (m_Containers[i].container == container)
            {
                ActivateContainer(i);
                return;
            }
        }
    }

    Container *Dock::GetActiveContainer() const
    {
        if (m_ActiveIndex < 0 || m_ActiveIndex >= (int)m_Containers.size())
            return nullptr;
        return m_Containers[m_ActiveIndex].container;
    }

    Container *Dock::GetContainer(int idx) const
    {
        if (idx < 0 || idx >= (int)m_Containers.size())
            return nullptr;
        return m_Containers[idx].container;
    }

    const std::string &Dock::GetContainerTitle(int idx) const
    {
        static const std::string s_Empty;
        if (idx < 0 || idx >= (int)m_Containers.size())
            return s_Empty;
        return m_Containers[idx].title;
    }

    void Dock::RecalcLayout()
    {
        // 基类默认实现：激活容器并显示
        ShowActiveContainer();
    }

    BoundaryId Dock::HitTestEdge(int x, int y, int thickness) const
    {
        if (!m_Layout)
            return InvalidBoundary;

        // 对 4 个槽里每个非 InvalidBoundary 的真实边界判命中。
        // left/right 是垂直边，比 x；top/bottom 是水平边，比 y。
        // 命中厚度至少覆盖该边界的缝隙（boundary.width/2），保证缝里点击也能拖。
        // 且只在边界的实际跨度 [start,end] 内命中（分段边界只对那一段可拖）。
        auto hitSide = [&](BoundaryId id, bool vertical) -> bool
        {
            if (id == InvalidBoundary)
                return false;
            const int pos = m_Layout->GetBoundaryPosition(id);
            if (pos < 0)
                return false;
            const auto *b = m_Layout->GetBoundary(id);
            const int hitT = b ? std::max(thickness, b->width / 2) : thickness;
            const int coord = vertical ? x : y;
            if (std::abs(coord - pos) > hitT)
                return false;
            if (!b)
                return true;
            // 跨度沿另一维度：垂直边看 y∈[start,end]*H；水平边看 x∈[start,end]*W
            const int layoutW = static_cast<int>(m_Layout->get_width());
            const int layoutH = static_cast<int>(m_Layout->get_height());
            const int spanLo = static_cast<int>(b->start * (vertical ? layoutH : layoutW));
            const int spanHi = static_cast<int>(b->end * (vertical ? layoutH : layoutW));
            const int spanCoord = vertical ? y : x;
            return spanCoord >= spanLo && spanCoord <= spanHi;
        };

        // 垂直（左右）边界
        if (hitSide(m_Boundary.left, /*vertical*/ true))
            return m_Boundary.left;
        if (hitSide(m_Boundary.right, /*vertical*/ true))
            return m_Boundary.right;
        // 水平（上下）边界
        if (hitSide(m_Boundary.top, /*vertical*/ false))
            return m_Boundary.top;
        if (hitSide(m_Boundary.bottom, /*vertical*/ false))
            return m_Boundary.bottom;

        return InvalidBoundary;
    }

    void Dock::OnPaint(Canvas *canvas)
    {
        if (!canvas)
            return;
        // 基类默认实现：绘制背景
        canvas->Clear(0xFF202124);
    }

    // ── 生命周期回调 ──
    void Dock::OnContainerAdded(Container *container)
    {
        // 基类默认实现：空，子类可重写
    }

    void Dock::OnContainerRemoved(Container *container)
    {
        // 基类默认实现：空，子类可重写
    }

    void Dock::OnAddedToLayout(DockLayout *layout)
    {
        // 基类默认实现：空，子类可重写
    }

    void Dock::OnRemovedFromLayout()
    {
        // 基类默认实现：空，子类可重写
    }

    // ── 核心布局管理实现 ──

    Dock *Dock::Split(Direction dir, float size)
    {
        // 检查是否可以分割
        if (!CanSplit(dir))
            return nullptr;

        // 创建新Dock
        Dock *newDock = CreateNewDock(dir);
        if (!newDock)
            return nullptr;

        // 设置新Dock的边界槽
        BoundaryId newBoundary = InvalidBoundary;

        // 根据方向计算新Boundary的位置和朝向
        BoundaryOrientation orientation;
        switch (dir)
        {
        case Direction::Left:
        case Direction::Right:
            orientation = BoundaryOrientation::Vertical;
            // 计算新Boundary的位置（size是比例）
            newBoundary = m_Layout->AddBoundary(orientation, size);
            break;
        case Direction::Top:
        case Direction::Bottom:
            orientation = BoundaryOrientation::Horizontal;
            newBoundary = m_Layout->AddBoundary(orientation, size);
            break;
        }

        if (newBoundary == InvalidBoundary)
        {
            delete newDock;
            return nullptr;
        }

        // 设置新旧Dock的边界槽关系
        switch (dir)
        {
        case Direction::Left:
            // 新Dock在左，新Dock的右边是newBoundary，旧Dock的左边是newBoundary
            newDock->GetBoundarySlots().right = newBoundary;
            m_Boundary.left = newBoundary;
            break;
        case Direction::Right:
            // 新Dock在右，新Dock的左边是newBoundary，旧Dock的右边是newBoundary
            newDock->GetBoundarySlots().left = newBoundary;
            m_Boundary.right = newBoundary;
            break;
        case Direction::Top:
            // 新Dock在上，新Dock的下边是newBoundary，旧Dock的上边是newBoundary
            newDock->GetBoundarySlots().bottom = newBoundary;
            m_Boundary.top = newBoundary;
            break;
        case Direction::Bottom:
            // 新Dock在下，新Dock的上边是newBoundary，旧Dock的下边是newBoundary
            newDock->GetBoundarySlots().top = newBoundary;
            m_Boundary.bottom = newBoundary;
            break;
        }

        // 设置血缘关系
        newDock->SetDockFather(this);
        newDock->SetMergedBoundary(newBoundary);
        SetMergedBoundary(newBoundary);

        // 将新Dock添加到布局
        m_Layout->AddDock(newDock);

        // 重排布局
        m_Layout->RecalcLayout();

        return newDock;
    }

    void Dock::Merge()
    {
        // 检查是否可以合并
        if (!CanMerge())
            return;

        // 执行合并
        PerformMerge();
    }

    // ── 辅助方法实现 ──

    Dock *Dock::CreateNewDock(Direction dir)
    {
        if (!m_Layout)
            return nullptr;

        // 创建新Dock，parent挂到m_Layout
        Dock *newDock = new Dock(m_Layout);
        if (!newDock)
            return nullptr;

        // 设置新Dock的基本属性
        newDock->SetDockLayout(m_Layout);
        newDock->SetAllowBoundaryDrag(m_AllowBoundaryDrag);
        newDock->SetAllowWindowDragIn(m_AllowWindowDragIn);
        newDock->SetAllowWindowDragOut(m_AllowWindowDragOut);
        newDock->SetAllowSelfSplit(m_AllowSelfSplit);
        newDock->SetMaxContainers(m_MaxContainers);
        newDock->SetCanCloseWindow(m_CanCloseWindow);

        return newDock;
    }

    void Dock::PerformMerge()
    {
        if (!m_Layout || m_MergedBoundary == InvalidBoundary)
            return;

        // 沿m_MergedBoundary找到共享它的邻居Dock
        for (auto *dock : m_Layout->GetDockList())
        {
            if (dock == this || !dock)
                continue;

            const auto &slots = dock->GetBoundarySlots();
            // 检查是否共享m_MergedBoundary
            if (slots.top == m_MergedBoundary ||
                slots.bottom == m_MergedBoundary ||
                slots.left == m_MergedBoundary ||
                slots.right == m_MergedBoundary)
            {
                // 找到邻居，进行空间合并
                // 这里简化处理：直接从布局中移除本Dock
                m_Layout->RemoveDock(this);
                m_Layout->RemoveBoundary(m_MergedBoundary);
                m_Layout->RecalcLayout();
                delete this;
                return;
            }
        }

        // 没找到邻居，直接移除
        m_Layout->RemoveDock(this);
        m_Layout->RemoveBoundary(m_MergedBoundary);
        m_Layout->RecalcLayout();
        delete this;
    }

    bool Dock::CanSplit(Direction dir) const
    {
        // 基本检查
        if (!m_Layout || !m_AllowSelfSplit)
            return false;

        // 检查是否超过最大承载数（如果有容器的话）
        if (!m_Containers.empty() && m_Containers.size() >= static_cast<size_t>(m_MaxContainers))
            return false;

        return true;
    }

    bool Dock::CanMerge() const
    {
        // 只有空的Dock才能合并
        if (!IsEmpty())
            return false;

        if (!m_Layout || m_MergedBoundary == InvalidBoundary)
            return false;

        return true;
    }

    // ── 私有辅助方法 ──

    void Dock::ShowActiveContainer()
    {
        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());

        for (int i = 0; i < (int)m_Containers.size(); ++i)
        {
            Container *c = m_Containers[i].container;
            if (!c)
                continue;

            if (i == m_ActiveIndex)
            {
                // 首次挂载：创建设为本窗口的子窗口
                if (!c->GetNativeHandle())
                {
                    c->SetParentHwnd(GetNativeHandle());
                    c->SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible |
                                      WindowStyleFlag::ClipChildren |
                                      WindowStyleFlag::ClipSiblings);
                    c->setSize(static_cast<uint>(std::max(0, width)),
                               static_cast<uint>(std::max(0, height)));
                    c->MoveAndResize(0, 0, std::max(0, width),
                                     std::max(0, height));
                    c->show(ShowCmd::Show);
                }
                else
                {
                    // 已创建：直接搬运到内容区并显示
                    c->MoveAndResize(0, 0, std::max(0, width),
                                     std::max(0, height));
                    c->show(ShowCmd::Show);
                }
            }
            else
            {
                if (c->GetNativeHandle())
                    c->show(ShowCmd::Hide);
            }
        }
    }

}