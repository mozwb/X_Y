#pragma once
#include "Component/Component.h"
#include <string>

namespace X_Y {

    class Label : public Component {
    public:
        Label() = default;

        void SetText(const char* text) { m_Text = text; }
        void SetText(const std::string& text) { m_Text = text; }
        const std::string& GetText() const { return m_Text; }

        void SetColor(uint32_t color) { m_Color = color; }
        uint32_t GetColor() const { return m_Color; }

        void OnPaint(Canvas& canvas) override;

        void SetBgColor(uint32_t color) { m_BgColor = color; }

    private:
        std::string m_Text;
        uint32_t m_Color = 0x00000000;
        uint32_t m_BgColor = 0x00FFFFFF;
    };

}
