#pragma once

#include "../dock/Dock.h"
#include "../UiCore/UIEvent.h"
#include "Widget/Canvas.h"
#include "XCore/FilesSystem/FilesSystem.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <functional>

namespace X_Y
{
    // ============================================================
    // DockLayout — 纯逻辑的布局管理器（规划 Dock 布局，不再 XWidget）
    //
    // 集中式 boundary 注册表 + Dock 的父树。
    //   - 持有全局一组 Boundary（分割线），Dock 用 4 槽引用来定位。
    //   - 持有 Dock 列表，RecalcLayout 里让每个 Dock 由自己边界换算矩形。
    //   - 提供拖分割线的纯逻辑驱动（OnMouse*），由壳(Container)喂坐标。
    //   - 提供 OnPaint(Canvas&) 画背景 + 分割线 + 各 Dock。
    // 所有窗口能力（Canvas/尺寸/鼠标）由外层 Container 壳驱动，本类只算。
    //
    // ── ⚠️ 所有权模型（2026-09-13 显式化，改代码前必读）──
    //
    //   Dock 有【两种】来源，所有权必须分清（砚台定案 (b)）：
    //
    //   ① 内建 Dock（借用，不拥有）
    //      形如 TopLayout 的五个 Dock —— 它们是 TopLayout 的【值成员】，
    //      生命周期由 C++ 自动管理，随 TopLayout 一起生灭。
    //      DockLayout 只通过 m_Docks 观察它们，【绝不 delete】。
    //
    //   ② 动态 Dock（拥有）
    //      Dock::Split() 里 new 出来的，以及 AddDock 传入的堆对象。
    //      登记在 m_OwnedDocks 里，由 DockLayout 负责 delete。
    //
    //   为什么必须区分：m_Docks 里两种混着放。若无脑 delete，
    //   会把 TopLayout 的值成员 delete 掉 → 立即崩溃。
    //
    //   登记规则：
    //     DockBind(dock&, ...)  → 内建语义（引用传入=调用方持有）→ 借用
    //     AddOwnedDock(dock*)   → 动态语义（指针传入=接管）→ 拥有
    //     Container 里 `new Dock()` 后必须用 AddOwnedDock。
    // ============================================================
    class DockLayout
    {
    public:
        DockLayout() = default;
        virtual ~DockLayout();

        // ── 宿主注入 ──
        void SetHostRepaint(std::function<void()> cb)
        {
            m_HostRepaint = std::move(cb);
            for (Dock *dock : m_Docks)
                if (dock)
                    dock->SetHostRepaint(m_HostRepaint);
        }
        std::function<void()> GetHostRepaint() const { return m_HostRepaint; }
        void SetActiveSize(int w, int h); // 壳的布局区域尺寸

        int GetLayoutWidth() const { return m_LayoutW; }
        int GetLayoutHeight() const { return m_LayoutH; }

        // ── boundary 注册表管理 ──
        BoundaryId AddBoundary(BoundaryOrientation orientation, float line,
                               BoundaryStatus status = BoundaryStatus::Movable);
        bool SetBoundaryColor(BoundaryId id, uint32_t color);
        bool SetBoundaryLine(BoundaryId id, float line);
        // 设置边界可移动范围。
        bool SetBoundaryRange(BoundaryId id, float min, float max);
        bool SetBoundaryWidth(BoundaryId id, int width);
        // 设置边界绘制跨度。
        bool SetBoundarySize(BoundaryId id, BoundaryId startBoundary,
                             BoundaryId endBoundary);
        bool GetBoundarySize(BoundaryId id, float &start, float &end) const;

        bool MoveBoundary(BoundaryId id, int delta);
        bool RemoveBoundary(BoundaryId id);

        const Boundary *GetBoundary(BoundaryId id) const;
        int GetBoundaryPosition(BoundaryId id) const; // 布局坐标（像素）

        // ── dock 管理 ──
        // 登记一个【内建】Dock：借用语义，不接管所有权。
        // 用于 TopLayout 那种"Dock 是值成员"的场景（见文件头所有权说明）。
        void AddDock(Dock *dock);

        // 登记一个【动态】Dock：接管所有权，析构时由 DockLayout 释放。
        // 用于 `new Dock()` / `Dock::Split()` 产生的 Dock。
        void AddOwnedDock(Dock *dock);

        bool DockBind(Dock &dock, BoundaryId top, BoundaryId bottom,
                      BoundaryId left, BoundaryId right);

        // 从布局中移除（不释放；调用方决定后续）。
        void RemoveDock(Dock *dock);

        // 从布局中移除【并释放】（仅对动态 Dock 有效；内建 Dock 会被忽略并告警）。
        void RemoveAndDestroyDock(Dock *dock);

        // 按落点收容一个已脱离宿主的 Panel；不销毁也不复制 Panel。
        // 落点命中哪个 Dock 就交给它，落空/不可加返回 nullptr（调用方负责回退）。
        Panel *AddPanelAt(Panel *panel, int x, int y,
                          const std::string &title = "");
        const std::vector<Dock *> &GetDockList() const { return m_Docks; }

        void SetBackgroundColor(uint32_t color) { m_BackgroundColor = color; }
        uint32_t GetBackgroundColor() const { return m_BackgroundColor; }

        // 重排所有 Dock（边界变化 / 布局 resize / Dock 内容变化时调用）
        void RecalcLayout();

        // ── 命中 + 拖分割线 ──
        BoundaryId HitTestBoundary(int x, int y, int thickness = 4) const;
        // 命中哪个 Dock（布局绝对坐标）。逆序遍历，与 OnPaint 的 z 序一致。
        Dock *HitTestDock(int x, int y) const;
        bool IsDraggingBoundary() const { return m_DraggingBoundary != InvalidBoundary; }

        // ── 输入（事件对象全链路）：e.x/e.y 为相对本布局的局部坐标。
        //    先判拖分割线（鼠标 Press 命中边界→进入拖拽态，Move→拖，Release→结束）。
        //    非拖拽态：命中命中的 Dock → 下传给其激活面板。
        void RouteInput(UIInputEvent &e);

        void RouteFileDragEnter(const std::vector<XPath> &files, int x, int y);
        void RouteFileDragOver(const std::vector<XPath> &files, int x, int y);
        void RouteFileDragLeave();
        void RouteFileDrop(const std::vector<XPath> &files, int x, int y);

        // ── 全局激活面板（供壳转发输入 / 命中）──
        Panel *GetActivePanel() const;

        // ── 绘制 ──
        virtual void OnPaint(Canvas &canvas);

    protected:
        bool IsValidBoundary(BoundaryId id) const;
        int BoundaryPosition(BoundaryId id) const;

        int m_LayoutW = 0, m_LayoutH = 0;
        std::vector<Boundary> m_Boundaries;

        // ⚠️ 两套列表，语义完全不同（见文件头所有权说明）：
        //   m_Docks      —— 【全部】Dock 的观察列表（内建 + 动态），绘制/命中使用。
        //                   里面的指针一律【借用】，本类绝不据此 delete。
        //   m_OwnedDocks —— 仅【动态】Dock（我 new/接管的），析构时逐个释放。
        std::vector<Dock *> m_Docks;
        std::vector<std::unique_ptr<Dock>> m_OwnedDocks;

        uint32_t m_BackgroundColor = 0xFF202124;

        BoundaryId m_DraggingBoundary = InvalidBoundary;
        int m_LastDragX = 0, m_LastDragY = 0;
        // 鼠标按下时锁定的 Dock（从按下到抬起，事件都发给它，避免拖出边界时丢失）。
        // ⚠️ 存【指针】而不是下标：早先用 `dock - m_Docks.front()` 算下标，
        //    依赖"dock 确实在 m_Docks 里"这个前提，脆弱且易错。
        Dock *m_MouseCaptureDock = nullptr;
        Panel *m_FileDropPanel = nullptr;

        std::function<void()> m_HostRepaint;
        void RequestRepaint()
        {
            if (m_HostRepaint)
                m_HostRepaint();
        }
    };

} // namespace X_Y
