#pragma once
#include "Widget/XWidget.h"
#include "Widget/Canvas.h"
#include "XCore/FilesSystem/FilesSystem.h"
#include <memory>
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
    //
    // ── ⚠️ 所有权模型（2026-09-13 显式化，改代码前必读）──
    //
    //   DockLayout 有两种来源，必须分清：
    //
    //   ① 自建（拥有）—— AddSinglePanel 里 EnsureLayout() new 出来的。
    //      存在 m_LayoutOwned 里，Container 析构时自动释放。
    //
    //   ② 外部传入（借用）—— SetDockLayout(&externalLayout) 传进来的。
    //      典型：test/src/main.cpp 传的是【栈上】的 TopLayout。
    //      Container 绝不能 delete 它，它由调用方管理。
    //
    //   区分由 m_LayoutOwned 是否为空来表达 —— 而不是原来那个
    //   m_OwnLayout 布尔标志（标志与实际状态可能不一致，unique_ptr 不会）。
    // ─────────────────────────────────────────────────────────────
    class Container : public XWidget
    {
    public:
        explicit Container(XWidget *parent = nullptr);
        ~Container() override;

        // ── 统一模型：总是挂一个纯逻辑 DockLayout ──
        // ⚠️ 传入的 layout 是【借用】：Container 不接管所有权（可以是栈对象）。
        //    若此前有自建的布局，会先被释放。
        virtual void SetDockLayout(DockLayout *layout); // 复杂场景：塞整个布局

        // 便捷：一行造一个"单面板工具窗"。内部自动 DockLayout → Dock → Panel。
        // ⚠️ panel 的所有权交给 Dock（由 Dock 的 unique_ptr 持有），
        //    调用方不要再 delete 它。
        virtual Panel *AddSinglePanel(Panel *panel, const std::string &title = "");

        DockLayout *GetDockLayout() const { return m_Layout; }

        // 让渡：把一个 Panel 交出去（拖进别的 Dock / 摘成独立窗）
        // 从本壳的 DockLayout 里脱出，返回 panel；调用方随后接管所有权。
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

        // ⚠️ 两个成员语义不同，别混：
        //   m_LayoutOwned —— 我 new 的布局（独占；析构自动释放）
        //   m_Layout      —— 借用指针，永远指向"当前生效的布局"
        //                    （要么是 m_LayoutOwned.get()，要么是外部传进来的）
        //   这样外部传入的栈对象布局不会被误删，自建的也不会泄漏。
        std::unique_ptr<DockLayout> m_LayoutOwned;
        DockLayout *m_Layout = nullptr;
    };

} // namespace X_Y
