#include "Component/Label.h"
#include "Widget/FontLibrary.h"

namespace X_Y {

    void Label::OnPaint(Canvas& canvas) {
        canvas.FillRect(GetX(), GetY(), GetWidth(), GetHeight(), m_BgColor);
        auto& font = FontLibrary::Instance().GetDefault();
        canvas.DrawText(font, GetX(), GetY(), m_Text.c_str(), m_Color);
    }

}
