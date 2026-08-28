#pragma once

#include "UI/dock/Dock.h"
#include "Widget/Canvas.h"
#include <cstddef>
#include <cstdint>
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
    // ============================================================
    class DockLayout
    {
    public:
        DockLayout() = default;
        virtual ~DockLayout();

        // ── 宿主注入 ──
        void SetHostRepaint(std::function<void()> cb) { m_HostRepaint = std::move(cb); }
        void SetActiveSize(int w, int h);   // 壳的布局区域尺寸

        int GetLayoutWidth() const { return m_LayoutW; }
        int GetLayoutHeight() const { return m_LayoutH; }

        // ── boundary 注册表管理 ──
        BoundaryId AddBoundary(BoundaryOrientation orientation, float line,
                               BoundaryStatus status = BoundaryStatus::Movable);
        bool SetBoundaryColor(BoundaryId id, uint32_t color);
        bool SetBoundaryLine(BoundaryId id, float line);
        bool SetBoundarySize(BoundaryId id, float min, float max);
        bool SetBoundaryWidth(BoundaryId id, int width);
        bool SetBoundaryRange(BoundaryId id, float start, float end);
        bool MoveBoundary(BoundaryId id, int delta);
        bool RemoveBoundary(BoundaryId id);

        const Boundary *GetBoundary(BoundaryId id) const;
        int GetBoundaryPosition(BoundaryId id) const;    // 布局坐标（像素）

        // ── dock 管理 ──
        void AddDock(Dock *dock);
        bool DockBind(Dock &dock, BoundaryId top, BoundaryId bottom,
                      BoundaryId left, BoundaryId right);
        void RemoveDock(Dock *dock);
        const std::vector<Dock *> &GetDockList() const { return m_Docks; }

        void SetBackgroundColor(uint32_t color) { m_BackgroundColor = color; }
        uint32_t GetBackgroundColor() const { return m_BackgroundColor; }

        // 重排所有 Dock（边界变化 / 布局 resize / Dock 内容变化时调用）
        void RecalcLayout();

        // ── 命中 + 拖分割线（壳喂布局坐标调用）──
        BoundaryId HitTestBoundary(int x, int y, int thickness = 4) const;
        bool IsDraggingBoundary() const { return m_DraggingBoundary != InvalidBoundary; }
        // 返回 true = 本布局已拦截（拖分割线）；false = 放行（应转发给激活 Panel）
        bool OnMousePressed(int x, int y);
        bool OnMouseMoved(int x, int y);
        void OnMouseReleased(int x, int y);

        // ── 全局激活面板（供壳转发输入 / 命中）──
        Panel *GetActivePanel() const;

        // ── 绘制 ──
        virtual void OnPaint(Canvas &canvas);

    protected:
        bool IsValidBoundary(BoundaryId id) const;
        int BoundaryPosition(BoundaryId id) const;
        bool HandleDrag(int x, int y);

        int m_LayoutW = 0, m_LayoutH = 0;
        std::vector<Boundary> m_Boundaries;
        std::vector<Dock *> m_Docks;
        uint32_t m_BackgroundColor = 0xFF202124;

        BoundaryId m_DraggingBoundary = InvalidBoundary;
        int m_LastDragX = 0, m_LastDragY = 0;

        std::function<void()> m_HostRepaint;
        void RequestRepaint() { if (m_HostRepaint) m_HostRepaint(); }
    };

} // namespace X_Y
