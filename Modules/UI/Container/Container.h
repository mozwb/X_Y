#pragma once
#include "Widget/XWidget.h"
#include "Widget/Canvas.h"
#include <vector>

namespace X_Y
{

    class Component;
    class Panel;

    // Container — 壳（ShellWidget）：一个带 HWND 的窗口容器。
    //
    // 它既能当"独立工具窗口"（不设父窗口时），也能当"Dock 里的面板"
    // （设置父窗口/HWND 后作为子窗口复用于 Dock），这就是"用设置父窗口
    // 来决定身份"的设计。
    //
    // 职责：
    //   1. 窗口层：继承 XWidget，握 HWND / Canvas / 系统消息（沿用旧实现）。
    //   2. 内容委托：可选地持有一个纯逻辑 Panel*；当不设 Panel 时，它自己也
    //      能直接 AddComponent 管理组件（旧行为保留，供 LogViewer/HexViewer
    //      override OnPaint/AddComponent 使用）。
    class Container : public XWidget
    {
    public:
        explicit Container(XWidget *parent = nullptr);
        virtual ~Container();

        // ── 内容层：可选挂一个纯逻辑 Panel（壳把绘制/输入委托给它）──
        void SetPanel(Panel *panel);
        Panel *GetPanel() const { return m_Panel; }

        void AddComponent(Component *comp);
        void RemoveComponent(Component *comp);
        void ClearComponents();

    protected:
        // 平台绘制回调
        // 窗口绘制有两种，如果是这种就是托管给消息循环绘制
        // 如果采用flush就是主动绘制
        void OnPaint(Canvas *canvas) override;
        void OnFileDragEnter(const std::vector<XPath> &files, int x, int y) override;
        void OnFileDragOver(const std::vector<XPath> &files, int x, int y) override;
        void OnFileDragLeave() override;
        void OnFileDrop(const std::vector<XPath> &files, int x, int y) override;
        Component *HitTest(int x, int y);
        std::vector<Component *> m_Components;

    private:
        // 拖拽等需要持续跟踪的交互目标（按下到抬起期间保持）

        Component *m_DragTarget = nullptr;
        Component *m_FileDragTarget = nullptr;
        int m_DragStartX = 0, m_DragStartY = 0;

        Panel *m_Panel = nullptr; // 可选内容层（壳委托给它画）
    };

}
