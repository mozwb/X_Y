#pragma once
#include "Component/Component.h"

namespace X_Y {

    class ScrollArea : public Component {
    public:
        ScrollArea() = default;

        void SetContent(Component* content) { m_Content = content; }
        Component* GetContent() const { return m_Content; }

        void SetScrollOffset(int offset);
        int GetScrollOffset() const { return m_ScrollOffset; }
        int GetContentHeight() const {
            return m_Content ? m_Content->GetHeight() : 0;
        }

        void ScrollBy(int delta) {
            SetScrollOffset(m_ScrollOffset - delta);
        }

        void OnPaint(Canvas& canvas) override;

    private:
        void ClampOffset();

        Component* m_Content = nullptr;
        int m_ScrollOffset = 0;
    };

}
