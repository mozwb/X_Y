#include "dock/Dock.h"
#include "DockLayout/DockLayout.h"

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
        // Container 的所有权在调用方；Dock 只负责管理/切换显示，不 delete。
        m_Containers.clear();
    }

    Container *Dock::AddContainer(Container *container, const std::string &title)
    {
        if (!container)
            return nullptr;

        ContainerEntry entry;
        entry.container = container;
        entry.title = title;
        m_Containers.push_back(std::move(entry));

        ActivateContainer((int)m_Containers.size() - 1);
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
                m_Containers.erase(it);

                // 若移除的是激活项，退到最后一个可用项
                if (m_ActiveIndex >= (int)m_Containers.size())
                    m_ActiveIndex = (int)m_Containers.size() - 1;

                // 变空 → 被动合并（并回 MergedBoundary 邻居，本 Dock 会被 delete）。
                // ⚠️ 调用后不要再触碰 this（Merge() 可能触发 remove/destroy）。
                if (m_Containers.empty())
                {
                    Merge();
                    return true;
                }
                if (!m_Containers.empty())
                    ActivateContainer(m_ActiveIndex >= 0 ? m_ActiveIndex : 0);
                return true;
            }
        }
        return false;
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

    void Dock::ActivateContainer(int idx)
    {
        if (idx < 0 || idx >= (int)m_Containers.size())
            return;

        m_ActiveIndex = idx;
        ShowActiveContainer();
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

    // 把激活的 Container 挂到 Dock 内容区（tab 栏以下）并显示，其余隐藏。
    void Dock::ShowActiveContainer()
    {
        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());
        const int contentY = m_TabBarHeight;
        const int contentH = height - m_TabBarHeight;

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
                               static_cast<uint>(std::max(0, contentH)));
                    c->MoveAndResize(0, contentY, std::max(0, width),
                                     std::max(0, contentH));
                    c->show(ShowCmd::Show);
                }
                else
                {
                    // 已创建：直接搬运到内容区并显示
                    c->MoveAndResize(0, contentY, std::max(0, width),
                                     std::max(0, contentH));
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

    void Dock::RecalcLayout()
    {
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
        // Dock 自身画背景（tab 栏位后续绘制，这里先铺底色留白）
        canvas->Clear(0xFF202124);
    }

    // （原 InSameSplitTree / CanMergeWith 随"合并改为被动"而删除：
    //  空 dock 只并回自己的 MergedBoundary 那条边对应的邻居，天然同宗，
    //  不再需要主动遍历求共享边。见 Dock.h Merge() 注释。）

    Dock *Dock::Split(Direction dir, float size)
    {
        // TODO：切割实现（见 dock/Dock.h Split 注释）。
        //   1) 校验 m_Layout && m_Splittable
        //   2) m_Layout->AddBoundary(...) 新增一条对应方向的分割线 → 得到 newBoundaryId
        //   3) new Dock(m_Layout) + SetDockLayout + SetDockFather(this)
        //   4) 把新旧两子共享这条新 Boundary（对应方向槽互换）
        //   5) 关键：本 Dock 和新 Dock 都 SetMergedBoundary(newBoundaryId)，
        //      这样空 dock 并回时能用它认到邻居（MergedBoundary=最新切割边）
        //   6) m_Layout->AddDock(newDock) + RecalcLayout
        //   7) return newDock
        (void)dir;
        (void)size;
        return nullptr;
    }

    void Dock::Merge()
    {
        // TODO：被动合并实现（见 dock/Dock.h Merge() 私有方法注释）。
        //   0) 前置：本 Dock 已空（m_Containers.empty()）。
        //   1) 沿 m_MergedBoundary 那条边找共享它的邻居 Dock（在 m_Layout 里找）。
        //   2) 把空 Dock 非 MergedBoundary 的外侧三条边交给邻居，让邻居接管空间。
        //   3) m_Layout->RemoveDock(this) + delete this；
        //      m_Layout->RemoveBoundary(m_MergedBoundary)。
        //   4) m_Layout->RecalcLayout()。
    }

}
