#pragma once
#include"window/src/windows/XWidget.h"
namespace X_Y {
	class GLWidget :public XWidget{
    public:
        GLWidget(XWidget* parent);
        ~GLWidget();

        // 初始化 OpenGL 上下文（像 GLFW 一样）
        bool initOpenGL();

        // 交换缓冲（渲染一帧）
        void swapBuffers();

    private:
        HGLRC m_hGLRC = nullptr;   // OpenGL 渲染上下文
        HDC m_hDC = nullptr;       // 设备上下文
    };
}