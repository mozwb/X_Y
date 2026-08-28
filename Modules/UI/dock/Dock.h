#pragma once

#include "UI/Container/Container.h"
#include "Widget/XWidget.h"
#include <string>
#include <vector>

namespace X_Y
{
    class DockLayout;

    // ============================================================
    // 布局边界类型（原在 DockLayout.h/旧 Dock.h 定义，现统一收在 Dock.h 以便
    // DockBoundary 槽引用；DockLayout.h include 本头后直接复用，不再重复定义）
    // ============================================================
    using BoundaryId = std::size_t;
    static constexpr BoundaryId InvalidBoundary = static_cast<BoundaryId>(-1);

    enum class BoundaryOrientation
    {
        Horizontal, // 水平分割线（分隔上/下）
        Vertical    // 垂直分割线（分隔左/右）
    };

    enum class BoundaryStatus : uint8_t
    {
        None = 0,
        Movable = 1U << 0
    };

    struct Boundary // 相对于整个布局设置
    {
        BoundaryOrientation orientation = BoundaryOrientation::Vertical;
        float line = 0.0f; // 0~1 相对的 t 值（乘以布局对应方向尺寸得到像素位置）
        BoundaryStatus status = BoundaryStatus::Movable;
        uint32_t color = 0xFF909090;
        uint32_t dragcolor = 0xFF00FF00;
        float min = 0.05f; // 活动范围下限（相对 t）
        float max = 0.95f; // 活动范围上限（相对 t）
        // 缝隙总宽（逻辑像素）：该边界两侧的布局里各露出一条 width/2 的缝，
        // 让分割线可见、且能在缝里显示。不同宗的 dock 之间 width 可设大些，
        // 同宗的可设小些，由调用方按宗分别 SetBoundaryWidth。默认 4。
        int width = 4;
        // 沿边界自身的跨度（0~1 相对，×布局对应方向尺寸）：
        //   水平线 → x 从 start 到 end；垂直线 → y 从 start 到 end。
        // 默认 [0,1] 贯穿整个布局。设短可做成"仅一段"的边界。
        float start = 0.0f;
        float end = 1.0f;
        bool removed = false; // 墓碑标记：true表示已移除，id保持稳定但跳过处理
    };

    // ============================================================
    // Dock — UI 四层结构中的第二层（DockLayout → Dock → Container → Component）
    //
    // Dock 是一个抽象基类，提供完整的布局管理框架。
    // 子类只需要定制界面表现和交互行为。
    //
    // 核心职责：
    //   1) 布局边界管理：维护4个边界槽，参与DockLayout的布局计算
    //   2) Split/Merge操作：提供完整的分割和合并逻辑
    //   3) 容器管理：基础的容器添加、删除、激活功能
    //   4) 血缘关系管理：记录父子关系，用于同宗合并判定
    //
    // 子类定制：
    //   - 界面表现：OnPaint() 绘制标签栏、标题栏等
    //   - 交互行为：拖拽、关闭、分割等行为的开关
    //   - 特定逻辑：容器变化后的特殊处理
    // ============================================================
    class Dock : public XWidget
    {
    public:
        // 切分方向：从当前 Dock 的哪个边往外/往内切出新一块。
        // 语义直观：往 Top/Left 切 = 新 Dock 在旧 Dock 的上方/左侧；
        //           往 Bottom/Right 切 = 新 Dock 在旧 Dock 的下方/右侧。
        enum class Direction
        {
            Top,
            Bottom,
            Left,
            Right
        };

        // 一个 Dock 的 4 个边界槽。每个槽指向：
        //   - 一条真实 Boundary（可移动/与其他 Dock 共享）；
        //   - 或 InvalidBoundary，表示该侧直贴 DockLayout 外框（由布局重排时
        //     用布局边缘，等价"固定不可移动"）。
        struct DockBoundary
        {
            BoundaryId top = InvalidBoundary;
            BoundaryId bottom = InvalidBoundary;
            BoundaryId left = InvalidBoundary;
            BoundaryId right = InvalidBoundary;
        };

    public:
        explicit Dock(XWidget *parent = nullptr);
        virtual ~Dock();

        // ── 核心布局管理（完整实现，子类无需重写）──
        Dock *Split(Direction dir, float size);
        void Merge();

        // ── 容器管理（基础实现，子类可扩展）──
        Container *AddContainer(Container *container, const std::string &title = "");
        bool RemoveContainer(Container *container);
        void ActivateContainer(int idx);
        void ActivateContainer(Container *container);
        Container *GetActiveContainer() const;
        Container *GetContainer(int idx) const;
        const std::string &GetContainerTitle(int idx) const;
        int GetContainerCount() const { return (int)m_Containers.size(); }
        int GetActiveIndex() const { return m_ActiveIndex; }
        bool IsEmpty() const { return m_Containers.empty(); }

        // ── 边界管理（提供访问接口）──
        const DockBoundary &GetBoundarySlots() const { return m_Boundary; }
        DockBoundary &GetBoundarySlots() { return m_Boundary; }
        void SetBoundarySlots(const DockBoundary &b) { m_Boundary = b; }

        // ── 血缘关系管理（提供访问接口）──
        void SetDockFather(Dock *f) { m_DockFather = f; }
        Dock *GetDockFather() const { return m_DockFather; }
        void SetMergedBoundary(BoundaryId id) { m_MergedBoundary = id; }
        BoundaryId GetMergedBoundary() const { return m_MergedBoundary; }

        // ── 布局相关（提供基础实现）──
        void RecalcLayout();
        BoundaryId HitTestEdge(int x, int y, int thickness = 4) const;

        // ── 状态设置（成员变量管理）──
        void SetAllowBoundaryDrag(bool allow) { m_AllowBoundaryDrag = allow; }
        void SetAllowWindowDragIn(bool allow) { m_AllowWindowDragIn = allow; }
        void SetAllowWindowDragOut(bool allow) { m_AllowWindowDragOut = allow; }
        void SetAllowSelfSplit(bool allow) { m_AllowSelfSplit = allow; }
        void SetMaxContainers(int max) { m_MaxContainers = max; }
        void SetCanCloseWindow(bool can) { m_CanCloseWindow = can; }

        // ── 状态查询（成员变量访问）──
        bool IsBoundaryDragAllowed() const { return m_AllowBoundaryDrag; }
        bool IsWindowDragInAllowed() const { return m_AllowWindowDragIn; }
        bool IsWindowDragOutAllowed() const { return m_AllowWindowDragOut; }
        bool IsSelfSplitAllowed() const { return m_AllowSelfSplit; }
        int GetMaxContainers() const { return m_MaxContainers; }

        // ── 虚函数接口（子类可重写）──
        // 复杂行为判断
        virtual bool CanCloseWindow() const { return m_CanCloseWindow && !IsEmpty(); }
        virtual int GetMaxContainers() const { return m_MaxContainers; }

        // 生命周期回调
        virtual void OnContainerAdded(Container *container);
        virtual void OnContainerRemoved(Container *container);
        virtual void OnAddedToLayout(DockLayout *layout);
        virtual void OnRemovedFromLayout();

        // 绘制接口（子类必须重写以绘制界面）
        virtual void OnPaint(Canvas *canvas) override;

        // ── 核心实现方法（供内部使用）──
        DockLayout *GetDockLayout() const { return m_Layout; }
        void SetDockLayout(DockLayout *layout) { m_Layout = layout; }

    protected:
        // ── 辅助方法 ──
        Dock *CreateNewDock(Direction dir);
        void PerformMerge();
        bool CanSplit(Direction dir) const;
        bool CanMerge() const;
        void ShowActiveContainer();

    private:
        // ── 核心数据 ──
        DockLayout *m_Layout = nullptr;
        Dock *m_DockFather = nullptr;                  // 同宗合并判定用的父（分割树的根=nullptr）
        DockBoundary m_Boundary;                       // 本 Dock 的 4 个边界槽
        BoundaryId m_MergedBoundary = InvalidBoundary; // 最近一次 split 产生的边（merge 时去掉它）

        // ── 容器管理 ──
        struct ContainerEntry
        {
            Container *container = nullptr;
            std::string title;
        };
        std::vector<ContainerEntry> m_Containers;
        int m_ActiveIndex = -1;

        // ── 状态变量 ──
        bool m_AllowBoundaryDrag = true;  // 是否允许边界拖拽
        bool m_AllowWindowDragIn = false; // 是否允许窗口拖入
        bool m_AllowWindowDragOut = true; // 是否允许窗口拖出
        bool m_AllowSelfSplit = true;     // 是否允许分割自己
        int m_MaxContainers = INT_MAX;    // 最大承载数
        bool m_CanCloseWindow = true;     // 是否允许关闭
    };

}