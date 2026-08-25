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
    };

    // ============================================================
    // Dock — UI 四层结构中的第二层（DockLayout → Dock → Container → Component）
    //
    // 一个 Dock 就是一个停靠"区域块"。它在内部直接持有多个 Container 做 tab
    // 切换（原 DockPanel 职责已合并进来，不再有独立 Panel 层）；作为 DockLayout
    // 的一个成员参与停靠布局，位置/尺寸由它身上的 4 个边界槽（boundary）决定。
    //
    // 职责分两部分（与你 Dock.h 草稿的规划一致）：
    //   1) panel 部分：持有 Container 列表做 tab（激活者显示在内容区，其余隐藏）。
    //      预留 m_TabBarHeight 顶部标签栏位（暂不绘制，后续补 TabBar）。
    //   2) dock 部分：自己是 DockLayout 的一部分。要再内部切割 = 从自身区域切出
    //      一个新 Dock 交给 DockLayout 管理。因此：
    //         - split() 在 DockLayout 里新增一条 Boundary 并切出一个新 Dock，
    //           新旧两个 Dock 共享这条新 Boundary（一个当好边界、一个当坏边界）。
    //         - merge() 把同宗（同一棵分割树）的两个相邻 Dock 合并回一个，去掉
    //           它们之间那条共享的 Boundary。
    //      拉伸（拖动 Boundary）由 DockLayout 统一处理（那条 Boundary 被多个
    //      Dock 共用，dragging 时整体重排），Dock 自己不拦截鼠标。
    //
    // 事件约定：Dock 不自己拦截鼠标事件。Win32 下鼠标点在最顶层覆盖的子窗口上，
    //           sender=该窗口，Dispatcher 按 sender 精确匹配、不冒泡——所以拖
	//           分割线这类"布局级"交互由 DockLayout 通过 Application 全局鼠标
	//           钩子（方案A）接管，Dock 自身不参与转发。
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
        ~Dock() override;

        // ── panel 部分：Container tab 管理（原 DockPanel 职责）──
        Container *AddContainer(Container *container,
                                const std::string &title = {});
        bool RemoveContainer(Container *container);

        void ActivateContainer(int idx);
        void ActivateContainer(Container *container);

        Container *GetActiveContainer() const;
        Container *GetContainer(int idx) const;
        const std::string &GetContainerTitle(int idx) const;
        int GetContainerCount() const { return (int)m_Containers.size(); }
        int GetActiveIndex() const { return m_ActiveIndex; }
        bool IsEmpty() const { return m_Containers.empty(); }

        // 内容区内容整体重排（对接父 DockLayout 的边界变化/自身 resize）
        void RecalcLayout();

        // ── tab 栏预留（暂不绘制）──
        void SetTabBarHeight(int h) { m_TabBarHeight = (h < 0 ? 0 : h); }
        int GetTabBarHeight() const { return m_TabBarHeight; }

        // ── dock 部分：关联 DockLayout 与边界槽 ──
        void SetDockLayout(DockLayout *layout) { m_Layout = layout; }
        DockLayout *GetDockLayout() const { return m_Layout; }

        const DockBoundary &GetBoundarySlots() const { return m_Boundary; }
        DockBoundary &GetBoundarySlots() { return m_Boundary; }
        void SetBoundarySlots(const DockBoundary &b) { m_Boundary = b; }

        // 分裂血缘：记录本 Dock 是从哪个 Dock 切割出来的（对应草稿 SetDockFather）。
        // 被动合并当前只用 MergedBoundary 找邻居，不强依赖它；保留作分裂树血缘，
        // 供未来"限制非同宗 dock 合并/再切"等扩展使用。设 nullptr = 顶层 Dock。
        void SetDockFather(Dock *f) { m_DockFather = f; }
        Dock *GetDockFather() const { return m_DockFather; }

        // 本 Dock 最新一次 split 产生的那条 Boundary（也是它变空时并回邻居的
        // 那条边）。每次 Split 时应把新产生的 BoundaryId 设进来。
        void SetMergedBoundary(BoundaryId id) { m_MergedBoundary = id; }
        BoundaryId GetMergedBoundary() const { return m_MergedBoundary; }

        // ── 切割 / 合并（dock 部分核心；本次写好接口与实现注释，逻辑留 TODO）──
        //
        // split(dir, size)：从本 Dock 沿 dir 方向切出一个新 Dock（主动行为）。
        //   实现步骤（后续迭代落实）：
        //     1. 校验：本 Dock 已在某个 DockLayout 里（m_Layout 非空）且允许分割。
        //     2. DockLayout 新增一条 Boundary：
        //           - dir 为 Left/Right → BoundaryOrientation::Vertical；
        //           - dir 为 Top/Bottom → BoundaryOrientation::Horizontal。
        //        line 取让"新 Dock 尺寸 ≈ size"的比例（相对布局对应方向尺寸）。
        //     3. new 一个 Dock*，parent 挂 m_Layout，SetDockLayout(m_Layout)，
        //        SetDockFather(this)（与新 Dock 同宗）。
        //     4. 把新旧两个 Dock 的对应边界槽指向这条新 Boundary：
        //           - dir=Left  ：新 Dock.right = 新Boundary；旧 Dock.left = 新Boundary
        //           - dir=Right ：新 Dock.left  = 新Boundary；旧 Dock.right = 新Boundary
        //           - dir=Top   ：新 Dock.bottom = 新Boundary；旧 Dock.top = 新Boundary
        //           - dir=Bottom：新 Dock.top   = 新Boundary；旧 Dock.bottom = 新Boundary
        //        （另一侧各自的槽保持不变，含 InvalidBoundary=贴外框。）
        //     5. 关键：把这条新 BoundaryId 用 SetMergedBoundary 同时写到本 Dock 和
        //        newDock（MergedBoundary=最新切割边），空 dock 并回时认它做邻居。
        //     6. DockLayout::AddDock(newDock) 加入管理，随后 RecalcLayout 重排两都。
        //     7. 返回 newDock。
        Dock *Split(Direction dir, float size);

        // ── 边界命中（由命中 Dock 上报"被拖的边"，供 DockLayout 拖动）──
        // Dock 变空时那条合并边要用的 Boundary 由 GetMergedBoundary 给（见上）。
        //
        // HitTestEdge(x, y, thickness)：给定相对 DockLayout 客户区的逻辑坐标，
        // 判断鼠标是否靠近本 Dock 的一条【真实边界】（非 InvalidBoundary 的槽）。
        //   判定：对本 Dock 4 个边界槽里每个非 InvalidBoundary 的槽，
        //         用 m_Layout->GetBoundaryPosition(id) 取它的像素位置；
        //         left/right 是垂直边（比较 x），top/bottom 是水平边（比较 y）。
        //         若 |坐标 - 边位置| <= thickness 则命中，返回该槽的 BoundaryId。
        //   为什么由 docker 上报而不是 layout 全局扫：同一朝向（尤其水平）多条
        //   boundary 像素级可能相邻/重叠，layout 全局 HitTestBoundary 会二义。
        //   Dock 只报自己真实拥有那条的真实 boundary，天然消歧（上下层各报各的），
        //   不会有"拖中间那层结果动到底层"的问题。
        BoundaryId HitTestEdge(int x, int y, int thickness = 4) const;

        // 是否允许被切割（供 UI 层开关，如某些 Dock 不允许再拆）
        void SetSplittable(bool on) { m_Splittable = on; }
        bool IsSplittable() const { return m_Splittable; }

    protected:
        void OnPaint(Canvas *canvas) override;

    private:
        struct ContainerEntry
        {
            Container *container = nullptr;
            std::string title;
        };

        // 被动合并（变空时自动并回 MergedBoundary 邻居）。
        // 语义：Dock 变空只有两种途径——(1) 用户关闭；(2) 最后一个 Container 被
        //       移走（搬到别的 Dock 或摘成悬浮）。两种情况都统一走"空了就并回"：
        //       RemoveContainer 移走最后一个时会自动调用，关闭路径同样调用。
        //   实现步骤（后续迭代落实，由 RemoveContainer/关闭路径触发）：
        //     1. 前置：本 Dock 必然为空（m_Containers.empty()）。
        //     2. 沿 m_MergedBoundary 那条边找到共享它的邻居 Dock（在 m_Layout 里找）。
        //     3. 把空 Dock 的非 MergedBoundary 那三条边对应的边界：
        //        外侧边（本 Dock 旧的 left/right/top/bottom 中非 MergedBoundary 的）
        //        交给邻居 Dock，让邻居接管空 Dock 占的空间。
        //     4. 从 DockLayout 移除本 Dock（RemoveDock），delete 本 Dock；并
        //        RemoveBoundary(m_MergedBoundary)（这条分割线已无用）。
        //     5. 触发 m_Layout->RecalcLayout() 重排。
        void Merge();

        // 激活窗口重新挂到 Dock 内容区并 show
        void ShowActiveContainer();

        std::vector<ContainerEntry> m_Containers;
        int m_ActiveIndex = -1;
        int m_TabBarHeight = 0;
        DockLayout *m_Layout = nullptr;
        Dock *m_DockFather = nullptr; // 同宗合并判定用的父（分割树的根=nullptr）
        DockBoundary m_Boundary;      // 本 Dock 的 4 个边界槽
        BoundaryId m_MergedBoundary = InvalidBoundary; // 最近一次 split 产生的边（merge 时去掉它）
        bool m_Splittable = true;
    };

}
