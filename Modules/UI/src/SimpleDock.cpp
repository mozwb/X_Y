#include "dock/SimpleDock.h"
#include "DockLayout/DockLayout.h"
#include "UI/Label.h"

namespace X_Y
{

    SimpleDock::SimpleDock(XWidget *parent)
        : Dock(parent)
    {
        // 设置SimpleDock的行为
        SetMaxContainers(1);          // 只支持单个Container
        SetAllowSelfSplit(false);     // 不允许分割
        SetAllowBoundaryDrag(false);  // 不允许边界拖拽
        SetAllowWindowDragIn(false);  // 不允许窗口拖入
        SetAllowWindowDragOut(true);  // 允许窗口拖出
        SetCanCloseWindow(true);      // 允许关闭
    }

    SimpleDock::~SimpleDock()
    {
        // 清理资源
    }

    void SimpleDock::OnPaint(Canvas *canvas)
    {
        if (!canvas)
            return;

        const int width = static_cast<int>(get_width());
        const int height = static_cast<int>(get_height());

        // 绘制背景
        canvas->Clear(0xFF2D2D30);

        // 绘制标题栏
        canvas->FillRect(0, 0, width, m_TitleBarHeight, 0xFF3E3E42);
        
        // 绘制标题栏文字
        auto* activeContainer = GetActiveContainer();
        std::string title = activeContainer ? GetContainerTitle(0) : "Empty Dock";
        canvas->DrawText(title, 10, m_TitleBarHeight / 2 - 8, 0xFFCCCCCC);
        
        // 绘制关闭按钮
        canvas->FillRect(width - 25, 5, 20, 20, 0xFF5A5A5C);
        canvas->DrawText("×", width - 20, m_TitleBarHeight / 2 - 8, 0xFFCCCCCC);
    }

    void SimpleDock::OnAddedToLayout(DockLayout *layout)
    {
        // 基类调用
        Dock::OnAddedToLayout(layout);
        
        // 如果是空的，可以添加一个默认容器
        if (IsEmpty())
        {
            auto* container = new Container(this);
            container->SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible |
                                    WindowStyleFlag::ClipChildren | WindowStyleFlag::ClipSiblings);
            
            auto* label = new Label("Content", container);
            label->SetPosition(10, 10);
            label->SetAutoSize(true);
            
            AddContainer(container, "Default");
        }
    }

    void SimpleDock::OnContainerAdded(Container *container)
    {
        // 基类调用
        Dock::OnContainerAdded(container);
        
        // 如果是第一个容器，激活它
        if (GetContainerCount() == 1)
        {
            ActivateContainer(0);
        }
    }

}