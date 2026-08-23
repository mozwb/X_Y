#include "Panel/DockPanel.h"
#include "UI/Container/Container.h"

namespace X_Y
{

    // ════════════════════════════════════════════════════════════
    // DockPanel
    // ════════════════════════════════════════════════════════════

    DockPanel::DockPanel(XWidget *parent)
        : XWidget(parent)
    {
        // DockPanel 是嵌入的子窗口

        SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible |
                       WindowStyleFlag::ClipChildren | WindowStyleFlag::ClipSiblings);
        if (parent && parent->GetNativeHandle())
        {
            SetParentHwnd(parent->GetNativeHandle());
        }
    }

    DockPanel::~DockPanel()
    {
        m_Tabs.clear();
    }

    DockTab *DockPanel::AddContainer(Container *container, const std::string &title)
    {
        return AddPanel(container, title);
    }

    DockTab *DockPanel::InsertContainer(int idx, Container *container, const std::string &title)
    {
        return InsertPanel(idx, container, title);
    }

    Container *DockPanel::ExtractContainer(DockTab *tab)
    {
        return dynamic_cast<Container *>(ExtractPanel(tab));
    }

    void DockPanel::Activate(Container *container)
    {
        for (int i = 0; i < (int)m_Tabs.size(); ++i)
        {
            if (m_Tabs[i].Panel == container)
            {
                ActivateTab(i);
                return;
            }
        }
    }

    DockTab *DockPanel::AddPanel(XWidget *panel, const std::string &title)
    {
        return InsertPanel((int)m_Tabs.size(), panel, title);
    }

    DockTab *DockPanel::InsertPanel(int idx, XWidget *panel, const std::string &title)
    {
        DockTab tab;
        tab.Title = title;
        tab.Panel = panel;
        if (idx < 0 || idx > (int)m_Tabs.size())
            idx = (int)m_Tabs.size();
        m_Tabs.insert(m_Tabs.begin() + idx, tab);

        ActivateTab(idx);
        return &m_Tabs[idx];
    }

    void DockPanel::RemovePanel(DockTab *tab)
    {
        for (auto it = m_Tabs.begin(); it != m_Tabs.end(); ++it)
        {
            if (&(*it) == tab)
            {
                m_Tabs.erase(it);
                break;
            }
        }
        if (m_ActiveTab >= (int)m_Tabs.size())
            m_ActiveTab = (int)m_Tabs.size() - 1;
    }

    XWidget *DockPanel::ExtractPanel(DockTab *tab)
    {
        XWidget *panel = nullptr;
        for (auto it = m_Tabs.begin(); it != m_Tabs.end(); ++it)
        {
            if (&(*it) == tab)
            {
                panel = it->Panel;
                m_Tabs.erase(it);
                break;
            }
        }
        if (m_ActiveTab >= (int)m_Tabs.size())
            m_ActiveTab = (int)m_Tabs.size() - 1;
        return panel;
    }

    void DockPanel::ActivateTab(int idx)
    {
        if (idx < 0 || idx >= (int)m_Tabs.size())
            return;
        m_ActiveTab = idx;
        for (int i = 0; i < (int)m_Tabs.size(); ++i)
        {
            if (m_Tabs[i].Panel)
                m_Tabs[i].Panel->show(i == idx ? ShowCmd::Show : ShowCmd::Hide);
        }
    }

    DockTab *DockPanel::GetActiveTab() const
    {
        if (m_ActiveTab < 0 || m_ActiveTab >= (int)m_Tabs.size())
            return nullptr;
        return const_cast<DockTab *>(&m_Tabs[m_ActiveTab]);
    }

    DockTab *DockPanel::GetTab(int idx) const
    {
        if (idx < 0 || idx >= (int)m_Tabs.size())
            return nullptr;
        return const_cast<DockTab *>(&m_Tabs[idx]);
    }

    const std::string &DockPanel::GetTabTitle(int idx) const
    {
        static std::string s_Empty;
        if (idx < 0 || idx >= (int)m_Tabs.size())
            return s_Empty;
        return m_Tabs[idx].Title;
    }

} // namespace X_Y
