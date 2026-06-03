#include "GL/GLWidget.h"
#include <Windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
    // 构造函数
    GLWidget::GLWidget(XWidget* parent)
        : XWidget(parent)  // 调用父类构造
    {
        // 初始化时先把 OpenGL 上下文置空
        m_hGLRC = nullptr;
        m_hDC = nullptr;
    }

    // 析构函数
    GLWidget::~GLWidget()
    {
        // 释放 OpenGL 资源
        if (m_hGLRC) {
            wglMakeCurrent(nullptr, nullptr);  // 解除当前上下文
            wglDeleteContext(m_hGLRC);        // 删除渲染上下文
            m_hGLRC = nullptr;
        }

        // DC 一般由窗口管理，这里不用手动删
    }

    // 初始化 OpenGL 上下文（核心！）
    bool GLWidget::initOpenGL()
    {
        // 1. 获取当前窗口的 DC
        m_hDC = GetDC(GetHwnd());  // 假设 XWidget 提供 winId() 返回 HWND

        if (!m_hDC) {
            return false;
        }

        // 2. 设置像素格式（必须设置，否则 OpenGL 无法工作）
        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,
            32,          // 32位色深
            0,0,0,0,0,0,
            0,
            0,
            0,
            0,0,0,0,
            16,          // 深度缓冲 16位
            0,
            0,
            PFD_MAIN_PLANE,
            0,
            0,0,0
        };

        // 选择最合适的像素格式
        int pixelFormat = ChoosePixelFormat(m_hDC, &pfd);
        if (!pixelFormat) {
            return false;
        }

        // 设置像素格式
        if (!SetPixelFormat(m_hDC, pixelFormat, &pfd)) {
            return false;
        }

        // 3. 创建 OpenGL 渲染上下文
        m_hGLRC = wglCreateContext(m_hDC);
        if (!m_hGLRC) {
            return false;
        }

        // 4. 把上下文绑定到当前线程
        if (!wglMakeCurrent(m_hDC, m_hGLRC)) {
            wglDeleteContext(m_hGLRC);
            m_hGLRC = nullptr;
            return false;
        }

        // 到这里 OpenGL 初始化成功！
        return true;
    }

    // 交换双缓冲 → 把渲染好的画面显示出来
    void GLWidget::swapBuffers()
    {
        if (m_hDC) {
            SwapBuffers(m_hDC);
        }
    }
#endif
}
