#pragma once
#include "Component/Component.h"
#include <string>

namespace X_Y {

    class Button : public Component {
    public:
        Button() = default;

        void SetText(const char* text) { m_Text = text; }
        const std::string& GetText() const { return m_Text; }

        void SetTextColor(uint32_t c) { m_TextColor = c; }
        void SetBgColor(uint32_t c) { m_BgColor = c; }
        void SetHoverColor(uint32_t c) { m_HoverColor = c; }

        void OnPaint(Canvas& canvas) override;

    private:
        std::string m_Text;
        uint32_t m_TextColor = 0x00000000;
        uint32_t m_BgColor = 0x00F0F0F0;
        uint32_t m_HoverColor = 0x00E0E0E0;
        bool m_MouseHover = false;
    };

}
