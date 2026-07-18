#include "dock/DockPanel.h"

namespace X_Y {

// ════════════════════════════════════════════════════════════
// DockPanel
// ════════════════════════════════════════════════════════════

DockPanel::DockPanel(XWidget* parent)
    : Container(parent)
{
}

DockPanel::~DockPanel()
{
    m_Tabs.clear();
}

DockTab* DockPanel::AddPanel(XWidget* panel, const std::string& title)
{
    return InsertPanel((int)m_Tabs.size(), panel, title);
}

DockTab* DockPanel::InsertPanel(int idx, XWidget* panel, const std::string& title)
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

void DockPanel::RemovePanel(DockTab* tab)
{
    for (auto it = m_Tabs.begin(); it != m_Tabs.end(); ++it) {
        if (&(*it) == tab) { m_Tabs.erase(it); break; }
    }
    if (m_ActiveTab >= (int)m_Tabs.size())
        m_ActiveTab = (int)m_Tabs.size() - 1;
}

XWidget* DockPanel::ExtractPanel(DockTab* tab)
{
    XWidget* panel = nullptr;
    for (auto it = m_Tabs.begin(); it != m_Tabs.end(); ++it) {
        if (&(*it) == tab) {
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
    if (idx < 0 || idx >= (int)m_Tabs.size()) return;
    m_ActiveTab = idx;
    for (int i = 0; i < (int)m_Tabs.size(); ++i) {
        if (m_Tabs[i].Panel)
            m_Tabs[i].Panel->show(i == idx ? SHOW : HIDE);
    }
}

DockTab* DockPanel::GetActiveTab() const
{
    if (m_ActiveTab < 0 || m_ActiveTab >= (int)m_Tabs.size())
        return nullptr;
    return const_cast<DockTab*>(&m_Tabs[m_ActiveTab]);
}

DockTab* DockPanel::GetTab(int idx) const
{
    if (idx < 0 || idx >= (int)m_Tabs.size()) return nullptr;
    return const_cast<DockTab*>(&m_Tabs[idx]);
}

const std::string& DockPanel::GetTabTitle(int idx) const
{
    static std::string s_Empty;
    if (idx < 0 || idx >= (int)m_Tabs.size()) return s_Empty;
    return m_Tabs[idx].Title;
}

int DockPanel::HitTestTab(int x, int y) const
{
    if (y < 0 || y > c_TabH) return -1;
    int cx = c_Pad;
    for (int i = 0; i < (int)m_Tabs.size(); ++i) {
        if (x >= cx && x < cx + c_TabW) return i;
        cx += c_TabW + c_Pad;
    }
    return -1;
}

void DockPanel::OnPaint(Canvas& canvas)
{
    int w = get_width();
    int h = get_height();
    // ── 背景 ──────────────────────────────────────────
    canvas.FillRect(0, 0, w, h, RGB(40, 40, 40));

    // ── Tab 标签栏 ────────────────────────────────────
    canvas.FillRect(0, 0, w, c_TabH, RGB(50, 50, 55));

    int x = c_Pad;
    for (int i = 0; i < (int)m_Tabs.size(); ++i) {
        bool active = (i == m_ActiveTab);
        if (active)
            canvas.FillRect(x, 0, c_TabW, c_TabH, RGB(70, 70, 80));
        canvas.DrawText(x + 4, 4, m_Tabs[i].Title.c_str(),
                        active ? RGB(255, 255, 255) : RGB(180, 180, 180));
        x += c_TabW + c_Pad;
    }

    // ── tab 栏底部分隔线 ──────────────────────────────
    canvas.FillRect(0, c_TabH, w, 1, RGB(60, 60, 65));

    // ── 子 Component（委托给 Container 基类绘制） ────
    Container::OnPaint(canvas);
}

} // namespace X_Y
