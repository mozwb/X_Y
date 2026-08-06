#pragma once
#include "UI/include/Component/Component.h"
#include <string>
#include <vector>
#include <functional>

namespace X_Y {

    // ── TagBar：横向筛选标签条（LeetCode 式 filter chips） ──
    // 展示一组已添加的关键词 tag，自动换行排布，hover 显示 ×、点击 × 删除。
    // 高度自适应：换行后总行数 × 行高，由宿主（LogViewer）布局读取定位下方组件。
    // 不做滚动（筛选规则正常不会多到几十条）；真需要时再扩展折叠/滚动。

    class TagBar : public Component {
    public:
        TagBar() = default;

        void AddTag(const std::string& tag);
        void RemoveTag(const std::string& tag);
        void Clear();

        int GetTagCount() const { return (int)m_Tags.size(); }
        const std::vector<std::string>& GetTags() const { return m_Tags; }

        void SetTagHeight(int h) { m_TagHeight = h; }

        // 排版并返回总高（需 Canvas 测字宽）。不绘制，供宿主布局前先量高。
        int Measure(Canvas& canvas);

        // 某个 tag 被点击 × 删除时回调（参数 = 被删关键词）
        std::function<void(const std::string&)> OnTagRemove;

        void OnPaint(Canvas& canvas) override;
        void OnMouseMoved(int localX, int localY) override;
        void OnMousePressed(int localX, int localY) override;
        void OnMouseReleased(int localX, int localY) override;

    private:
        struct Rect { int x, y, w, h; };

        // 命中 tag：返回索引（-1 未命中）
        int HitTest(int x, int y) const;
        // 是否命中某 tag 的 × 区域
        bool IsInClose(int index, int x, int y) const;

        std::vector<std::string> m_Tags;
        std::vector<Rect> m_TagRects;      // 每 tag 整体矩形（局部坐标），与 tags 对齐（hover 命中用）
        std::vector<Rect> m_CloseRects;    // 每 tag 的 × 点击区域（局部坐标），与 tags 对齐
        int m_HoverIndex = -1;             // 当前鼠标悬浮的 tag

        int m_TagHeight = 24;   // 单行 tag 高
        int m_Gap = 6;          // tag 间横向间距
        int m_PadX_text = 8;    // tag 内文字左内边距
        int m_CloseW = 14;      // 保留（未用，删除按钮区尺寸现由 Measure 内部计算）
        int m_MarginY = 2;      // 上下留白
    };

}
