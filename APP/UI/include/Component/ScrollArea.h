#pragma once
#include "UI/include/Component/Component.h"
#include "Widget/include/Dpi.h"

namespace X_Y {

    class ScrollArea : public Component {
    public:
        ScrollArea() = default;

        void SetContent(Component* content) { m_Content = content; }
        Component* GetContent() const { return m_Content; }

        void SetScrollOffset(int offset);
        int GetScrollOffset() const { return m_ScrollOffset; }

        // 内容总高（换行/布局后算出的总高，由内容自治提供）
        int GetContentHeight() const {
            return m_Content ? m_Content->GetHeight() : 0;
        }

        // 滚动一步的像素粒度（内容自治提供：列表=行高）
        int GetScrollStep() const {
            return m_Content ? m_Content->GetScrollStep() : 1;
        }

        // 可视区宽度（扣除滑块占位后，内容实际可用宽度）
        int GetViewWidth() const { return GetWidth() - kScrollbarWidth; }

        void ScrollBy(int delta) {
            SetScrollOffset(m_ScrollOffset - delta);
        }

        // 滚轮输入：yDelta>0 向上滚(看更早)，<0 向下滚
        // yDelta 是物理滚轮单位，需转逻辑(÷scale)再乘逻辑步长，
        // 否则 150% 屏下次滚动量偏大。
        void OnScroll(float yDelta) override {
            float s = Dpi::GetScale();
            ScrollBy((int)(yDelta / s * GetScrollStep()));
        }

        // 滑块交互：拖动 + 点击轨道翻页

        void OnMousePressed(int localX, int localY) override;
        void OnMouseMoved(int localX, int localY) override;
        void OnMouseReleased(int localX, int localY) override;

        void OnPaint(Canvas& canvas) override;

    private:
        void ClampOffset();
        bool NeedsScrollbar() const;
        // 输出滑块矩形（ScrollArea 局部坐标）
        void GetThumbRect(int& outY, int& outH) const;
        void DrawScrollbar(Canvas& canvas) const;

        Component* m_Content = nullptr;
        int m_ScrollOffset = 0;
        static constexpr int kScrollbarWidth = 8;

        // 滑块拖拽状态
        bool m_DraggingThumb = false;
        int m_DragOffsetY = 0;   // 按下点距滑块顶部的偏移
        static constexpr int kMinThumbH = 16;
    };

}
