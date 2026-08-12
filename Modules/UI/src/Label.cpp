#include "Component/Label.h"

namespace X_Y {

    void Label::OnPaint(Canvas& canvas) {
        canvas.FillRect(GetX(), GetY(), GetWidth(), GetHeight(), m_BgColor);
        canvas.DrawText(GetX(), GetY(), m_Text.c_str(), m_Color);
    }

}
