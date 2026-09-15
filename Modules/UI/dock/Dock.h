#pragma once

#include "../Panel/Panel.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstddef>
#include <cstdint>

namespace X_Y
{
    class DockLayout;

    // ============================================================
    // 布局边界类型
    // boundary 本体由 DockLayout 集中注册表持有（可被多个 Dock 共享）。
    // 每个 Dock 通过它身上的 4 个槽（DockBoundary）引用若干 boundary 来定位。
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

    struct Boundary
    {
        BoundaryOrientation orientation = BoundaryOrientation::Vertical;
        float line = 0.0f; // 0~1 相对 t（×布局对应方向尺寸 = 像素位置）
        BoundaryStatus status = BoundaryStatus::Movable;
        uint32_t color = 0xFF909090;
        uint32_t dragcolor = 0xFF00FF00;
        float min = 0.05f;
        float max = 0.95f;
        int width = 4; // 缝隙总宽（两侧各露 width/2）
        BoundaryId startBoundary = InvalidBoundary;
        BoundaryId endBoundary = InvalidBoundary;
        bool removed = false; // 墓碑
    };

    // 每个 Dock 引用的 4 条边界槽（可与邻居共享同一条）
    struct DockBoundary
    {
        BoundaryId top = InvalidBoundary;
        BoundaryId bottom = InvalidBoundary;
        BoundaryId left = InvalidBoundary;
        BoundaryId right = InvalidBoundary;
    };

    // ============================================================
    // Dock — 纯逻辑的多面板宿主（一个区域，可 tab，可切割/合并）
    //
    // Dock 不碰窗口（不再是 XWidget）。它持有:
    //   - 多个 Panel（tab 切换）
    //   - 4 个边界槽（DockBoundary）→ 由自己边界定位
    //   - 绘制自己（tab 栏 + 激活 Panel）→ 画到宿主给的 Canvas&
    // 所有窗口能力（Canvas/尺寸/鼠标）由外层宿主（Container 壳 → DockLayout）驱动。
    //
    // ── ⚠️ 所有权模型（2026-09-13 显式化，改代码前必读）──
    //
    //   Dock 【拥有】它持有的 Panel：m_Panels 是 unique_ptr 容器。
    //   这与 DockLayout→Dock 的"两种来源"不同 —— Panel 一律是堆对象，
    //   没有值成员的情况，所以统一独占，没有借用/拥有之分。
    //
    //   面板的"换主人"（拖拽）走 std::move：从 A 的 unique_ptr 转移到 B，
    //   转移期间【不能有任何人持有它】—— 见下面各级借用指针的断链约定。
    //
    //   借用（不拥有，但会在对象消失时被清空）：
    //     m_MouseCapturePanel —— 按下到抬起期间锁定的事件目标
    //   这两个必须在 AddPanel/RemovePanel/DetachPanel 时维护，
    //   否则会对着已经搬走的 Panel 调 OnInput（use-after-free）。
    // ============================================================
    class Dock
    {
    public:
        enum class Direction
        {
            Top,
            Bottom,
            Left,
            Right
        };

        Dock() = default;
        virtual ~Dock();

        Dock(const Dock &) = delete;
        Dock &operator=(const Dock &) = delete;

        // ── 重绘回调（宿主注入）──
        void SetHostRepaint(std::function<void()> cb) { m_HostRepaint = std::move(cb); }
        std::function<void()> GetHostRepaint() const { return m_HostRepaint; }
        void SetDockLayout(DockLayout *layout)
        {
            m_Layout = layout;
        }
        DockLayout *GetDockLayout() const { return m_Layout; }

        // ── 边界槽（由自己的 boundary 定位）──
        const DockBoundary &GetBoundarySlots() const { return m_Boundary; }
        DockBoundary &GetBoundarySlots() { return m_Boundary; }

        // ── 布局（宿主喂总尺寸，Dock 由自己边界换算位置矩形）──
        void SetActiveRect(int w, int h);
        void RecalcRect(); // 用边界 + 布局总尺寸算 m_X/m_Y/m_W/m_H
        int GetX() const { return m_X; }
        int GetY() const { return m_Y; }
        int GetWidth() const { return m_W; }
        int GetHeight() const { return m_H; }
        bool IsBoundaryPositionValid(BoundaryId id, float line) const;
        void SetMinimumWidth(float width) { m_MinWidth = width; }
        void SetMinimumHeight(float height) { m_MinHeight = height; }
        float GetMinimumWidth() const { return m_MinWidth; }
        float GetMinimumHeight() const { return m_MinHeight; }

        // ── 面板显示区域 ──
        // 坐标相对 Dock 左上角；默认区域为整个 Dock（菜单栏由 Dock 自动避让）。
        void SetPanelArea(int x, int y, int w, int h);
        void GetPanelArea(int &x, int &y, int &w, int &h) const;

        // 生效面板区（钳到 Dock 内后的实际值）。绘制原点 / 输入偏移 / 命中判定都用它。
        int GetEffPanelX() const { return m_EffPanelX; }
        int GetEffPanelY() const { return m_EffPanelY; }
        int GetEffPanelW() const { return m_EffPanelW; }
        int GetEffPanelH() const { return m_EffPanelH; }

        // 设置 Dock 顶部菜单/Tab 栏高度。面板区域会自动从该高度之后开始。
        void SetMenuBarHeight(int height);
        int GetMenuBarHeight() const { return m_MenuBarHeight; }

        // ── Tab 栏几何（绘制与命中必须共用，避免两处各写一份导致对不上）──
        static constexpr int kTabWidth = 120; // 单个 tab 的宽度
        int TabBarTotalWidth() const { return (int)m_Panels.size() * kTabWidth; }
        // 命中哪个 tab（x 为 Dock 局部坐标）；未命中返回 -1
        int TabIndexAt(int x) const
        {
            if (x < 0 || x >= TabBarTotalWidth())
                return -1;
            return x / kTabWidth;
        }

        // ── 面板管理（tab）──
        // ★ 所有权从签名上就能读出来：
        //   AddPanel(unique_ptr)     —— 接管所有权（调用点必须显式 std::move）
        //   DetachPanel -> unique_ptr —— 交出所有权（调用方拿到即拥有）
        //   没有任何一步经由"无主裸指针"过渡。

        // 接管一个 Panel（存进 unique_ptr）。成功返回非空（= 同一指针的借用视图）。
        Panel *AddPanel(std::unique_ptr<Panel> panel, const std::string &title = "");

        // 移除并【释放】该 Panel（析构 + 删除）。
        bool RemovePanel(Panel *panel);

        // 摘出：交出所有权，返回 unique_ptr；找不到返回 nullptr。
        // 典型用途：拖到别的 Dock / 摘成独立窗口。
        // ⚠️ 返回的 unique_ptr 若在接管前析构（早退/抛异常），Panel 会被正确释放，
        //    不会泄漏 —— 这正是它比"返回裸指针"安全的地方。
        std::unique_ptr<Panel> DetachPanel(Panel *panel, std::string *title = nullptr);

        bool ContainsPanel(const Panel *panel) const;
        void ActivatePanel(int idx);
        bool ActivatePanel(Panel *panel);
        Panel *GetActivePanel() const;
        Panel *GetPanel(int idx) const;
        const std::string &GetPanelTitle(int idx) const;
        int GetPanelCount() const { return (int)m_Panels.size(); }
        int GetActiveIndex() const { return m_ActiveIndex; }
        bool IsEmpty() const { return m_Panels.empty(); }

        // ── 同宗（切割/合并判定）──
        void SetDockFather(Dock *f) { m_DockFather = f; }
        Dock *GetDockFather() const { return m_DockFather; }
        void SetMergedBoundaryId(BoundaryId id) { m_MergedBoundaryId = id; }
        BoundaryId GetMergedBoundaryId() const { return m_MergedBoundaryId; }

        // ── 分屏 / 切割 / 合并（Dock 的"重新划分自己"职责）──
        bool IsSplittable() const { return m_Splittable; }
        void SetSplittable(bool v) { m_Splittable = v; }

        // 主动切一块：dir 方向，size 为相对比例(0~1)。
        // 返回新生成的对侧 Dock*，它接管当前激活面板；原 Dock 保留剩余面板(若有)。
        Dock *Split(Direction dir, float size);
        // 被动合并：本 Dock 变空后调用，沿 mergedBoundary 并回对侧邻居并删掉自己。
        void Merge();

        // ── 绘制（宿主给画布）──
        virtual void OnPaint(Canvas &canvas);

        // ── 命中（宿主算好布局坐标后调用）──
        BoundaryId HitTestEdge(int x, int y, int thickness = 4) const;
        Panel *HitTestPanel(int x, int y) const;

        // ── 输入（事件对象下传）：命中 tab 栏→切 tab；否则下传给激活 Panel ──
        virtual void RouteInput(UIInputEvent &e);
        bool CanAddPanel() const
        {
            return m_MaxPanelCount < 0 || (int)m_Panels.size() < m_MaxPanelCount;
        }
        void SetMaxPanelCount(int max) { m_MaxPanelCount = max; }

    protected:
        void ShowActivePanel();
        void UpdatePanelRects();
        // 内部：按索引摘下一条记录（不释放、不重排），返回其所有权并维护
        // 激活下标 + 清理借用指针。Remove/Detach/TakePanelInternal 共用。
        std::unique_ptr<Panel> TakeEntry(int idx, std::string *title);
        // 内部：摘出指定面板但不重排（Split 用，把活跃面板切给新 Dock）。
        // 返回所有权，调用方负责交给新 Dock；找不到返回 nullptr。
        std::unique_ptr<Panel> TakePanelInternal(Panel *panel);

        int m_X = 0, m_Y = 0, m_W = 100, m_H = 100;
        int m_LayoutW = 0, m_LayoutH = 0; // 宿主喂的布局总尺寸
        // ── 面板区（两套，务必分清）──
        // m_Panel*    : 声明值（SetPanelArea/SetMenuBarHeight/SetActiveRect 维护），
        //               跨 resize 存活，语义是"我希望面板占多大"。
        // m_EffPanel* : 生效值（UpdatePanelRects 把声明值钳到 Dock 内算出），
        //               绘制原点、输入偏移、命中判定【都用它】，保证三者同一份数据。
        int m_PanelX = 0, m_PanelY = 0;
        int m_PanelW = 100, m_PanelH = 76;
        int m_EffPanelX = 0, m_EffPanelY = 0;
        int m_EffPanelW = 100, m_EffPanelH = 76;
        int m_MenuBarHeight = 0;
        bool m_PanelAreaCustomized = false;
        float m_MinWidth = 0.05f;
        float m_MinHeight = 0.05f;
        int m_MaxPanelCount = -1; // 最大面板数，为负数表示不限制

        // ★ 拥有：Dock 独占这些 Panel，析构时自动释放。
        struct PanelEntry
        {
            std::unique_ptr<Panel> Panel_;
            std::string Title;
        };
        std::vector<PanelEntry> m_Panels;
        int m_ActiveIndex = -1;

        DockLayout *m_Layout = nullptr;
        Dock *m_DockFather = nullptr; // 同宗（分割树根=nullptr）
        BoundaryId m_MergedBoundaryId = InvalidBoundary;
        bool m_Splittable = true; // 是否允许切分自己

        DockBoundary m_Boundary; // 我引用的 4 条边界
        Panel *m_MouseCapturePanel = nullptr;

        std::function<void()> m_HostRepaint;
        void RequestRepaint()
        {
            if (m_HostRepaint)
                m_HostRepaint();
        }
    };

} // namespace X_Y
