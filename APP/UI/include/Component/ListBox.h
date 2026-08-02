#pragma once
#include "UI/include/Component/Component.h"
#include <string>
#include <vector>

namespace X_Y {

    struct ListBoxItem {
        std::string text;
        uint32_t textColor = 0x00000000;
        uint32_t bgColor = 0x00FFFFFF;
        void* userData = nullptr;
    };

    class ListBox : public Component {
    public:
        ListBox() = default;

        void AddItem(const char* text, uint32_t textColor = 0x00000000);
        void Clear();

        int GetItemCount() const { return (int)m_Items.size(); }
        const ListBoxItem& GetItem(int index) const { return m_Items[index]; }

        int GetSelectedIndex() const { return m_SelectedIndex; }
        void SetSelectedIndex(int idx) { m_SelectedIndex = idx; }

        int GetLineHeight() const { return m_LineHeight; }
        void SetLineHeight(int h);

        int GetScrollStep() const override { return m_LineHeight; }

        // 记录滚动视口，OnPaint 只画可视区行
        void SetViewport(int scrollOffset, int viewHeight) override {
            m_ViewOffset = scrollOffset;
            m_ViewHeight = viewHeight;
        }

        int GetRowFromMouseY(int localY) const;

        void OnPaint(Canvas& canvas) override;

    private:
        void UpdateHeight();

        std::vector<ListBoxItem> m_Items;
        int m_SelectedIndex = -1;
        int m_LineHeight = 18;
        int m_ViewOffset = 0;    // 滚动偏移（像素）
        int m_ViewHeight = 0;    // 可视区高（像素）
    };

}
