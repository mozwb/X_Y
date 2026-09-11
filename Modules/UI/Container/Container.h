#pragma once
#include "Widget/XWidget.h"
#include "Widget/Canvas.h"
#include "XCore/FilesSystem/FilesSystem.h"
#include <string>

namespace X_Y
{

    class Panel;
    class Dock;
    class DockLayout;

    // Container — 壳（ShellWidget）：一个带 HWND 的窗口，统一持有 DockLayout。
    //
    // 单一模型：Container 永远持有一个 DockLayout（纯逻辑布局）。DockLayout 规划
    // 若干 Dock（不同宗），Dock 持有多个 Panel。因此：
    //   - 独立工具窗口  = Container + DockLayout + 一个 Dock + 一个 Panel
    //   - Dock 宿主窗口 = Container + DockLayout + 多个 Dock + 各 Dock 的多个 Panel
    // 两者只是 DockLayout 里装了多少内容的差别，Container 永远是同一个壳。
    //
    // Container 只干三件事（壳的职责）：
    //   1. 提供 Canvas（XWidget 自带）→ 转给 DockLayout::OnPaint
    //   2. 提供尺寸（客户区）→ DockLayout::SetActiveSize
    //   3. 转发输入（Movement）→ 转布局坐标 → DockLayout::OnMouse*
    //
    // Container 自身【不再管理组件】——组件只能在 Panel 里。DockLayout/Dock 也只
    // 是宿主，不含组件。HexViewer/LogViewer 等旧 composite 后续迁到 Panel 形态。
    class Container : public XWidget
    {
    public:
        explicit Container(XWidget *parent = nullptr);
        ~Container() override;

        // ── 统一模型：总是挂一个纯逻辑 DockLayout ──
        virtual void SetDockLayout(DockLayout *layout); // 复杂场景：塞整个布局

        // 便捷：一行造一个"单面板工具窗"。内部自动 DockLayout → Dock → Panel。
        // 返回生成的布局，供后续再 add 更多面板；panel 所有权交托给 Dock。
        virtual Panel *AddSinglePanel(Panel *panel, const std::string &title = "");

        DockLayout *GetDockLayout() const { return m_Layout; }

        // 让渡：把一个 Panel 交出去（拖进别的 Dock / 摘成独立窗）
        // 从本壳的 DockLayout 里脱出，返回 panel；调用方随后接管。
        Panel *DetachPanel(Panel *panel, std::string *title = nullptr);

    protected:
        virtual Dock *CreateSinglePanelDock();

        void OnPaint(Canvas *canvas) override; // 壳把画布交给 DockLayout
        void OnFileDragEnter(const std::vector<XPath> &files, int x, int y) override;
        void OnFileDragOver(const std::vector<XPath> &files, int x, int y) override;
        void OnFileDragLeave() override;
        void OnFileDrop(const std::vector<XPath> &files, int x, int y) override;

    private:
        // 尺寸/输入转发（由 WndProc 消息经 XWidget 回调触发）
        void OnWindowResize();
        void EnsureLayout(); // 懒建默认 DockLayout

        DockLayout *m_Layout = nullptr;
        bool m_OwnLayout = false; // 是否是我 new 的默认布局
    };

} // namespace X_Y
