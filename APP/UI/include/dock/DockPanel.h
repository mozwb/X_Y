#pragma once

#include "UI/include/Container/Container.h"
#include "XCore/include/XYCore.h"
#include <string>
#include <vector>

namespace X_Y {

// ============================================================
// DockPanel — 单个停靠面板
// 只负责管理 tab 增删切换、摘出浮动
// ============================================================

struct DockTab {
    std::string Title;
    XWidget*    Panel = nullptr;
};

class DockPanel : public Container {
public:
    DockPanel(XWidget* parent);
    ~DockPanel() override;

    // ── Tab 管理 ────────────────────────────────────────
    DockTab* AddPanel(XWidget* panel, const std::string& title);
    DockTab* InsertPanel(int idx, XWidget* panel, const std::string& title);
    void     RemovePanel(DockTab* tab);
    XWidget* ExtractPanel(DockTab* tab);
    void     ActivateTab(int idx);

    int      GetTabCount() const { return (int)m_Tabs.size(); }
    int      GetActiveIndex() const { return m_ActiveTab; }
    DockTab* GetActiveTab() const;
    DockTab* GetTab(int idx) const;
    const std::string& GetTabTitle(int idx) const;
    bool     IsEmpty() const { return m_Tabs.empty(); }

protected:
    void OnPaint(Canvas& canvas) override;
    int  HitTestTab(int x, int y) const;

private:
    int m_ActiveTab = -1;
    std::vector<DockTab> m_Tabs;

    static constexpr int c_TabH = 24;
    static constexpr int c_TabW = 120;
    static constexpr int c_Pad  = 8;
};

} // namespace X_Y
