#pragma once

#include "UI/Dock/Dock.h"
#include <vector>

namespace X_Y
{

    // DockLayout 只协调多个 Dock 的位置，不规定具体的窗口区域方案。
    class DockLayout
    {
    public:
        void AddDock(Dock *dock);
        void RemoveDock(Dock *dock);
        void SetDockRect(Dock *dock, int x, int y, int w, int h);
        void RecalcLayout();

    private:
        struct Entry
        {
            Dock *dock = nullptr;
            int x = 0;
            int y = 0;
            int w = 0;
            int h = 0;
        };

        std::vector<Entry> m_Docks;
    };

}