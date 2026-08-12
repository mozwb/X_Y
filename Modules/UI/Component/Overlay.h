#pragma once
#include "Component.h"
#include <cstdint>

namespace X_Y {

    // Overlay — 半透明覆盖层，纯视觉组件，无事件处理
    // 用于停靠预览、选中高亮、区域指示等场景
    
    class Overlay : public Component {
    public:
        Overlay() = default;

        void SetColor(uint32_t color) { m_Color = color; }
        uint32_t GetColor() const { return m_Color; }

        void SetBorderColor(uint32_t color) { m_BorderColor = color; }
        uint32_t GetBorderColor() const { return m_BorderColor; }

        void SetBorderWidth(int w) { m_BorderWidth = w; }
        int GetBorderWidth() const { return m_BorderWidth; }

        void OnPaint(Canvas& canvas) override;

    private:
        uint32_t m_Color = 0x40FFFFFF;       // 半透明白色
        uint32_t m_BorderColor = 0x80808080;  // 半透灰色边框
        int m_BorderWidth = 2;
    };

}
