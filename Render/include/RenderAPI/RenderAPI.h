#pragma once
#include"renderMath/renderMath.h"
#include"XCore/include/XYCore.h"
#include"GraphicsContext/include/GraphicsType.h"
namespace X_Y {
    class RenderAPI {
    public:
        virtual ~RenderAPI() = default;
        virtual void Init() = 0;                // 初始化
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void SetClearColor(const RenderMath::Vec4& color) = 0; // 设置清除颜色
        virtual void SetLineWidth(float width) = 0;
        virtual void Cleanup() = 0;            // 释放资源
        virtual void Clear() = 0;
       static  Scope<RenderAPI> Create(GraphicsType e);
    };



}