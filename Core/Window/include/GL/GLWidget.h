#pragma once
#include"XWidget.h"
namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
	class GLWidget :public XWidget{
    public:
        GLWidget(XWidget* parent=nullptr);
        ~GLWidget();

        // 初始化 OpenGL 上下文（像 GLFW 一样）
        bool initOpenGL();

        // 交换缓冲（渲染一帧）
        void swapBuffers();

    private:
        HGLRC m_hGLRC = nullptr;   // OpenGL 渲染上下文
        HDC m_hDC = nullptr;       // 设备上下文
    };
#endif
}