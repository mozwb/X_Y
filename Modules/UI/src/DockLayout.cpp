#include "DockLayout/DockLayout.h"

#include <algorithm>

namespace X_Y
{

    void DockLayout::AddDock(Dock *dock)
    {
        if (!dock)
            return;
        const auto it = std::find_if(m_Docks.begin(), m_Docks.end(),
                                     [dock](const Entry &entry)
                                     { return entry.dock == dock; });
        if (it == m_Docks.end())
            m_Docks.push_back({dock});
    }

    void DockLayout::RemoveDock(Dock *dock)
    {
        m_Docks.erase(std::remove_if(m_Docks.begin(), m_Docks.end(),
                                     [dock](const Entry &entry)
                                     { return entry.dock == dock; }),
                      m_Docks.end());
    }

    void DockLayout::SetDockRect(Dock *dock, int x, int y, int w, int h)
    {
        AddDock(dock);
        for (auto &entry : m_Docks)
        {
            if (entry.dock == dock)
            {
                entry.x = x;
                entry.y = y;
                entry.w = w;
                entry.h = h;
                return;
            }
        }
    }

    void DockLayout::RecalcLayout()
    {
        for (const auto &entry : m_Docks)
        {
            if (!entry.dock)
                continue;
            entry.dock->MoveAndResize(entry.x, entry.y, entry.w, entry.h);
            entry.dock->RecalcLayout();
        }
    }

}