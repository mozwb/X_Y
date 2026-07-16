#pragma once
#include "Component/Component.h"
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

        int GetRowFromMouseY(int localY) const;

        void OnPaint(Canvas& canvas) override;

    private:
        void UpdateHeight();

        std::vector<ListBoxItem> m_Items;
        int m_SelectedIndex = -1;
        int m_LineHeight = 18;
    };

}
