#pragma once
#include "Component/Component.h"
#include <string>

namespace X_Y {

    class TextInput : public Component {
    public:
        TextInput() = default;

        void SetText(const char* text);
        void SetText(const std::string& text);
        const std::string& GetText() const { return m_Text; }

        void SetPlaceholder(const char* text) { m_Placeholder = text; }
        void SetTextColor(uint32_t c) { m_TextColor = c; }
        void SetBgColor(uint32_t c) { m_BgColor = c; }
        void SetBorderColor(uint32_t c) { m_BorderColor = c; }
        void SetReadOnly(bool r) { m_ReadOnly = r; }
        bool IsReadOnly() const { return m_ReadOnly; }

        void OnPaint(Canvas& canvas) override;
        void OnKeyDown(Input_t::KeyCode key) override;
        void OnChar(wchar_t ch) override;

    private:
        std::string m_Text;
        std::string m_Placeholder;
        int m_CursorPos = 0;
        uint32_t m_TextColor = 0x00000000;
        uint32_t m_BgColor = 0x00FFFFFF;
        uint32_t m_BorderColor = 0x00CCCCCC;
        bool m_ReadOnly = false;
    };

}
