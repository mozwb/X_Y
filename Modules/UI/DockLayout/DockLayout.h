#pragma once

#include "UI/dock/Dock.h"
#include "Widget/XWidget.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace X_Y
{
    // ============================================================
    // DockLayout — UI 四层结构的顶层（DockLayout → Dock → Container → Component）
    //
    // 集中式 boundary 模型：DockLayout 维护全局一组 Boundary（分割线），每个
    // Dock 通过它身上的 4 个边界槽（Dock::DockBoundary）指向这些 Boundary 来
    // 定位。拖动某条 Boundary 只改变它自己的 line，RecalcLayout 统一重排所有
    // 引用它的 Dock。
    //
    // Dock 切割（Dock::Split）产生的 Boundary 也集中存在这里；新 Dock 与旧 Dock
    // 共享那条新 Boundary，都作为 DockLayout 的子窗口由其管理位置（保证同宗
    // 才能合并的校验在 Dock 层做）。
    //
    // 事件约定（方案A）：Dock/DockLayout 都是独立子窗口，Win32 鼠标点在最顶层
    // 覆盖窗口上、sender=该窗口，Dispatcher 严格按 sender 匹配不冒泡。因此
    // "拖分割线"这类布局级交互由 Application 的全局鼠标钩子接管：DockLayout
    // 构造时注册 HandleGlobalMouse，用屏幕坐标→布局坐标→遍历 Dock::HitTestEdge
    // 判定命中边界并驱动拖动；只有命中边界热区才拦截，否则放行给内部窗口。
    // ============================================================
    class DockLayout : public XWidget
    {
    public:
        explicit DockLayout(XWidget *parent = nullptr);
        ~DockLayout() override;

        // ── boundary 管理 ──
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
        int GetBoundaryPosition(BoundaryId id) const;

        // ── dock 管理 ──
        void AddDock(Dock *dock);
        bool DockBind(Dock &dock, BoundaryId top, BoundaryId bottom,
                      BoundaryId left, BoundaryId right);
        void RemoveDock(Dock *dock);

        void SetBackgroundColor(uint32_t color);
        uint32_t GetBackgroundColor() const { return m_BackgroundColor; }

        // 重排所有 Dock（boundary 变化 / 布局自身 resize / Dock 内容变化时调用）
        void RecalcLayout();

        // ── 事件钩子（方案A：由 Application 全局鼠标钩子驱动分割线拖动）──
        // DockLayout 构造时把 HandleGlobalMouse 注册为 Application 的全局鼠标重定向
        // 钩子，析构时清除。钩子用屏幕坐标→布局客户区坐标→遍历本布局的每个
        // Dock::HitTestEdge 找命中边界（共享边天然同 id，同朝向多条边界不误伤）。
        //   说明：Application 钩子槽是"单一占用式"。本实现按"一个 DockLayout 管一个
        //         UI 主窗口"处理（构造注册/析构清除）。若需多布局同时存在，需升级为集合。
        // 命中判定：把某逻辑坐标(x,y 相对布局客户区)映射到命中的 boundary。
        BoundaryId HitTestBoundary(int x, int y, int thickness = 4) const;
        bool IsDraggingBoundary() const { return m_DraggingBoundary != InvalidBoundary; }
        BoundaryId GetDraggingBoundary() const { return m_DraggingBoundary; }
        void BeginDragBoundary(BoundaryId id);
        void EndDragBoundary();

    protected:
        void OnPaint(Canvas *canvas) override;

    private:
        // 布局窗口 resize 时重排
        void OnWindowResize();

        // 画布局背景 + 所有边界分割线（OnPaint(异步 WM_PAINT) 调用）
        void DrawLayout(Canvas &canvas);
        // 拖动即时局部刷新：把整窗画到常驻离屏 GetCanvas()，再 FlushArea 只上屏
        // 所有分割线所在窄带的包围盒（局部上屏，比整窗 Flush 快；画布复用不 new）。
        void RedrawBoundaryLines();
        // 计算所有分割线窄带的包围盒（像素，逻辑坐标），无可见边界返回 false
        bool BoundaryRectsBounds(int &x, int &y, int &w, int &h) const;

        bool IsValidBoundary(BoundaryId id) const;
        int BoundaryPosition(BoundaryId id) const;
        // 把某个边界槽解析为布局里的像素坐标（InvalidBoundary=贴外框）
        int HorizontalSidePosition(BoundaryId id, bool isRight) const;
        int VerticalSidePosition(BoundaryId id, bool isBottom) const;

        // 全局鼠标钩子处理器（绑定给 Application）。只处理按下/移动/抬起：
        //   命中边界热区 → 拦截并驱动分割线拖动；未命中 → 返回 false 放行给内部窗口。
        bool HandleGlobalMouse(const XMovement &event);

        std::vector<Dock *> m_Docks;   // 归属本布局的所有 Dock（位置由边界槽决定）
        std::vector<Boundary> m_Boundaries; // 全局分割线集合
        uint32_t m_BackgroundColor = 0xFF202124;
        BoundaryId m_DraggingBoundary = InvalidBoundary;
        int m_LastDragMouseX = 0;   // 拖动中上次鼠标（布局逻辑坐标）
        int m_LastDragMouseY = 0;
    };

}
