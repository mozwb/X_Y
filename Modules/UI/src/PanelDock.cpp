#include "dock/PanelDock.h"
#include "DockLayout/DockLayout.h"

#include <algorithm>

namespace X_Y
{

    PanelDock::PanelDock(XWidget *parent)
        : Dock(parent)
    {
        // PanelDock默认允许分割
        SetSplittable(true);
    }

    PanelDock::~PanelDock()
    {
        // Container的所有权在调用方；PanelDock只负责管理/切换显示，不delete。
        m_Containers.clear();
    }

    Container* PanelDock::AddContainer(Container* container, const std::string& title)
    {
        if (!container)
            return nullptr;

        ContainerEntry entry;
        entry.container = container;
        entry.title = title;
        entry.isWindowContainer = false;  // 默认不是从外部拖入的窗口
        m_Containers.push_back(std::move(entry));

        ActivateContainer((int)m_Containers.size() - 1);
        return container;
    }

    bool PanelDock::RemoveContainer(Container* container)
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

                // 变空 → 自动合并
                if (m_Containers.empty())
                {
                    PerformAutoMerge();
                    return true;
                }
                
                // 激活当前容器
                if (!m_Containers.empty())
                    ActivateContainer(m_ActiveIndex >= 0 ? m_ActiveIndex : 0);
                
                return true;
            }
        }
        return false;
    }

    void PanelDock::ActivateContainer(Container* container)
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

    void PanelDock::ActivateContainer(int idx)
    {
        if (idx < 0 || idx >= (int)m_Containers.size())
            return;

        m_ActiveIndex = idx;
        ShowActiveContainer();
    }

    Container* PanelDock::GetActiveContainer() const
    {
        if (m_ActiveIndex < 0 || m_ActiveIndex >= (int)m_Containers.size())
            return nullptr;
        return m_Containers[m_ActiveIndex].container;
    }

    Container* PanelDock::GetContainer(int idx) const
    {
        if (idx < 0 || idx >= (int)m_Containers.size())
            return nullptr;
        return m_Containers[idx].container;
    }

    const std::string& PanelDock::GetContainerTitle(int idx) const
    {
        static const std::string s_Empty;
        if (idx < 0 || idx >= (int)m_Containers.size())
            return s_Empty;
        return m_Containers[idx].title;
    }

    bool PanelDock::CanAcceptWindowDrag(int x, int y) const
    {
        // 检查是否在内容区域内（标签栏以下）
        if (y < m_TabBarHeight)
            return false;
        
        // 检查当前是否为空（空的PanelDock可以接受拖入）
        if (IsEmpty())
            return true;
            
        // 或者检查是否在激活的容器区域内
        auto* active = GetActiveContainer();
        if (active)
        {
            int activeY = m_TabBarHeight;
            int activeH = static_cast<int>(get_height()) - activeY;
            return (y >= activeY && y <= activeY + activeH);
        }
        
        return false;
    }

    bool PanelDock::OnWindowDropped(XWidget* window, int x, int y)
    {
        if (!window)
            return false;

        // 创建一个新的Container来包装这个窗口
        Container* container = new Container(this);
        if (!container)
            return false;

        // 设置容器属性
        container->SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible |
                                 WindowStyleFlag::ClipChildren | WindowStyleFlag::ClipSiblings);
        container->SetParentHwnd(GetNativeHandle());
        
        // 将窗口作为容器的子窗口
        window->SetParentHwnd(container->GetNativeHandle());
        window->SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible);
        window->show(ShowCmd::Show);

        // 添加容器到PanelDock
        std::string title = "Window " + std::to_string(m_Containers.size() + 1);
        AddContainer(container, title);
        
        return true;
    }

    bool PanelDock::CanCloseWindow() const
    {
        // 如果只有一个容器，不允许关闭（否则会变空合并）
        return m_Containers.size() > 1;
    }

    bool PanelDock::CloseWindow()
    {
        if (!CanCloseWindow())
            return false;

        // 关闭当前激活的容器
        auto* active = GetActiveContainer();
        if (active)
        {
            // 从PanelDock中移除（会触发隐藏和自动合并逻辑）
            return RemoveContainer(active);
        }
        
        return false;
    }

    XWidget* PanelDock::GetActiveWindow() const
    {
        auto* active = GetActiveContainer();
        if (!active)
            return nullptr;
            
        // 返回容器内的第一个子窗口（假设只有一个主窗口）
        // 这里简化处理，实际应该有更复杂的窗口管理逻辑
        return active->GetChildWindow();
    }

    Dock* PanelDock::Split(Direction dir, float size)
    {
        // 检查是否可以分割
        if (!CanSplit(dir))
            return nullptr;

        // 使用基类的Split方法创建新Dock
        Dock* newDock = Dock::Split(dir, size);
        if (!newDock)
            return nullptr;

        // 如果是PanelDock，创建一个新的PanelDock
        PanelDock* newPanelDock = dynamic_cast<PanelDock*>(newDock);
        if (newPanelDock)
        {
            // 设置新PanelDock的属性
            newPanelDock->SetSplittable(IsSplittable());
        }

        return newDock;
    }

    void PanelDock::Merge()
    {
        // PanelDock的合并：如果变空，自动执行合并
        if (IsEmpty())
        {
            PerformAutoMerge();
        }
    }

    void PanelDock::OnAddedToLayout(DockLayout* layout)
    {
        // 通知基类
        Dock::OnAddedToLayout(layout);
        
        // 如果是空的，可以立即接受拖入
        if (IsEmpty())
        {
            // 可以在这里设置一些视觉提示，表明可以接受拖入
        }
    }

    void PanelDock::OnRemovedFromLayout()
    {
        // 清理资源
        m_Containers.clear();
        m_ActiveIndex = -1;
        
        // 通知基类
        Dock::OnRemovedFromLayout();
    }

    void PanelDock::OnPaint(Canvas* canvas)
    {
        if (!canvas)
            return;

        // 绘制背景
        canvas->Clear(0xFF202124);

        // 绘制标签栏
        if (!m_Containers.empty())
        {
            canvas->FillRect(0, 0, static_cast<int>(get_width()), m_TabBarHeight, 0xFF404244);
            
            // 绘制标签栏文字（简化处理，实际需要更复杂的绘制）
            for (int i = 0; i < (int)m_Containers.size(); ++i)
            {
                const auto& entry = m_Containers[i];
                if (i == m_ActiveIndex)
                {
                    // 激活标签的样式
                    canvas->FillRect(0, i * 20, static_cast<int>(get_width()), 20, 0xFF606064);
                }
                // 这里可以添加文字绘制逻辑
            }
        }
    }

    bool PanelDock::ShouldAutoMerge() const
    {
        return IsEmpty() && GetDockLayout() != nullptr;
    }

    void PanelDock::PerformAutoMerge()
    {
        if (ShouldAutoMerge())
        {
            // 使用基类的PerformMerge方法
            PerformMerge();
        }
    }

    void PanelDock::ShowActiveContainer()
    {
        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());
        const int contentY = m_TabBarHeight;
        const int contentH = height - m_TabBarHeight;

        for (int i = 0; i < (int)m_Containers.size(); ++i)
        {
            Container* c = m_Containers[i].container;
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

}