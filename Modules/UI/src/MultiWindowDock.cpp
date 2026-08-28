#include "dock/MultiWindowDock.h"
#include "DockLayout/DockLayout.h"
#include "UI/Label.h"
#include "UI/Button.h"

namespace X_Y
{

    MultiWindowDock::MultiWindowDock(XWidget *parent)
        : Dock(parent)
    {
        // 设置MultiWindowDock的行为
        SetMaxContainers(10);         // 最多支持10个Container
        SetAllowSelfSplit(true);      // 允许分割
        SetAllowBoundaryDrag(true);   // 允许边界拖拽
        SetAllowWindowDragIn(true);   // 允许窗口拖入
        SetAllowWindowDragOut(true);  // 允许窗口拖出
        SetCanCloseWindow(true);      // 允许关闭
    }

    MultiWindowDock::~MultiWindowDock()
    {
        // 清理资源
    }

    void MultiWindowDock::OnPaint(Canvas *canvas)
    {
        if (!canvas)
            return;

        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());

        // 绘制背景
        canvas->Clear(0xFF1E1E1E);

        // 绘制tab栏
        DrawTabBar(canvas);

        // 绘制内容区域背景
        int contentY = m_TabBarHeight;
        int contentHeight = height - contentY;
        canvas->FillRect(0, contentY, width, contentHeight, 0xFF252526);
    }

    void MultiWindowDock::DrawTabBar(Canvas *canvas)
    {
        const int width = static_cast<int>(get_width());
        const int tabHeight = m_TabBarHeight;
        
        // 绘制tab栏背景
        canvas->FillRect(0, 0, width, tabHeight, 0xFF2D2D30);
        
        int tabX = 0;
        for (int i = 0; i < GetContainerCount(); ++i)
        {
            auto* container = GetContainer(i);
            if (!container)
                continue;

            const std::string& title = GetContainerTitle(i);
            const bool isActive = (i == GetActiveIndex());
            
            // 计算tab宽度
            int tabWidth = 120; // 固定宽度，实际应该根据文字长度计算
            
            // 绘制tab背景
            if (isActive)
            {
                canvas->FillRect(tabX, 0, tabWidth, tabHeight, 0xFF007ACC);
            }
            else
            {
                canvas->FillRect(tabX, 0, tabWidth, tabHeight, 0xFF3E3E42);
            }
            
            // 绘制tab文字
            canvas->DrawText(title, tabX + 8, tabHeight / 2 - 8, 
                           isActive ? 0xFFFFFFFF : 0xFFCCCCCC);
            
            // 绘制关闭按钮
            DrawCloseButton(canvas, tabX + tabWidth - 20, 2, isActive);
            
            tabX += tabWidth;
        }
    }

    void MultiWindowDock::DrawCloseButton(Canvas *canvas, int x, int y, bool isActive)
    {
        const int size = 16;
        
        // 绘制关闭按钮背景
        canvas->FillRect(x, y, size, size, isActive ? 0xFF007ACC : 0xFF5A5A5C);
        
        // 绘制×符号
        canvas->DrawText("×", x + 5, y + 2, 0xFFFFFFFF);
    }

    void MultiWindowDock::OnContainerAdded(Container *container)
    {
        // 基类调用
        Dock::OnContainerAdded(container);
        
        // 如果是第一个容器，激活它
        if (GetContainerCount() == 1)
        {
            ActivateContainer(0);
        }
        
        // 通知布局重绘
        if (GetDockLayout())
        {
            GetDockLayout()->RequestRepaint();
        }
    }

    void MultiWindowDock::OnContainerRemoved(Container *container)
    {
        // 基类调用
        Dock::OnContainerRemoved(container);
        
        // 通知布局重绘
        if (GetDockLayout())
        {
            GetDockLayout()->RequestRepaint();
        }
    }

    bool MultiWindowDock::CanCloseWindow() const
    {
        // 只有多个容器时才允许关闭
        return GetContainerCount() > 1;
    }

    int MultiWindowDock::GetMaxContainers() const
    {
        return 10; // 最多10个tab
    }

}