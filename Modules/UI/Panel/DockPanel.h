#pragma once

#include "Widget/XWidget.h"
#include "XCore/XYCore.h"
#include <string>
#include <vector>

namespace X_Y
{

    class Container;

    // ============================================================
    // DockPanel — Dock 中的内容切换层。
    // 不绘制切换器，只负责承载并切换内容窗口。
    // ============================================================

    struct DockTab
    {
        std::string Title;
        XWidget *Panel = nullptr;
    };

    class DockPanel : public XWidget
    {
    public:
        explicit DockPanel(XWidget *parent = nullptr);
        ~DockPanel() override;

        DockTab *AddContainer(Container *container, const std::string &title = {});
        DockTab *InsertContainer(int idx, Container *container, const std::string &title = {});
        Container *ExtractContainer(DockTab *tab);
        void Activate(Container *container);

        // 兼容旧的 XWidget 内容接口；新代码优先使用 Container 接口。
        DockTab *AddPanel(XWidget *panel, const std::string &title);
        DockTab *InsertPanel(int idx, XWidget *panel, const std::string &title);
        void RemovePanel(DockTab *tab);
        XWidget *ExtractPanel(DockTab *tab);
        void ActivateTab(int idx);

        int GetTabCount() const { return (int)m_Tabs.size(); }
        int GetActiveIndex() const { return m_ActiveTab; }
        DockTab *GetActiveTab() const;
        DockTab *GetTab(int idx) const;
        const std::string &GetTabTitle(int idx) const;
        bool IsEmpty() const { return m_Tabs.empty(); }

    private:
        int m_ActiveTab = -1;
        std::vector<DockTab> m_Tabs;
    };

} // namespace X_Y
