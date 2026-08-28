#pragma once
#include "Component.h"
#include <functional>
#include <string>

namespace X_Y
{

    class Button : public Component
    {
    public:
        Button() = default;

        void SetText(const char *text) { m_Text = text; }
        const std::string &GetText() const { return m_Text; }

        void SetTextColor(uint32_t c) { m_TextColor = c; }
        void SetBgColor(uint32_t c) { m_BgColor = c; }
        void SetHoverColor(uint32_t c) { m_HoverColor = c; }
        void SetRounded(bool rounded) { m_Rounded = rounded; }
        void SetCloseable(bool closeable) { m_Closeable = closeable; }
        void SetOnClick(std::function<void()> callback) { m_OnClick = std::move(callback); }
        void SetOnClose(std::function<void()> callback) { m_OnClose = std::move(callback); }

        void OnPaint(Canvas &canvas) override;
        void OnInput(UIInputEvent &e) override;

    private:
        std::string m_Text;
        uint32_t m_TextColor = 0xFF000000;
        uint32_t m_BgColor = 0xFFF0F0F0;
        uint32_t m_HoverColor = 0xFFE0E0E0;
        bool m_MouseHover = false;
        bool m_Rounded = false;
        bool m_Closeable = false;
        std::function<void()> m_OnClick;
        std::function<void()> m_OnClose;
    };

}
