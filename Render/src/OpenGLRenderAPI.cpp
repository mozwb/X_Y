#include"Render/include/OpenGl/OpenGLRenderAPI.h"
namespace X_Y {
    // OpenGL 初始化
    void OpenGLRenderAPI::Init()
    {
        // 初始化 OpenGL 状态（示例：开启深度测试）
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // 开启混合（透明渲染）
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // 设置视口
    void OpenGLRenderAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(x, y, width, height);
    }

    // 设置清屏颜色
    void OpenGLRenderAPI::SetClearColor(const RenderMath::Vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    // 设置线宽
    void OpenGLRenderAPI::SetLineWidth(float width)
    {
        glLineWidth(width);
    }

    // 释放资源
    void OpenGLRenderAPI::Cleanup()
    {

    }
    void OpenGLRenderAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    RApiType OpenGLRenderAPI::getType() {
        return RApiType::OpenGL;
    }
}