#include"GraphicsContext.h"

namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS

    OpenglGrContext::OpenglGrContext(HWND hwnd)
        : m_hWnd(hwnd), m_hDC(nullptr), m_hGLRC(nullptr)
    {
        m_hDC = ::GetDC(m_hWnd);
        if (!m_hDC) {
            XFATAL("GetDC 失败");
            return;
        }
        SetupPixelFormat();
    }

    OpenglGrContext::~OpenglGrContext()
    {
        if (m_hGLRC) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(m_hGLRC);
            m_hGLRC = nullptr;
        }
        if (m_hWnd && m_hDC) {
            ::ReleaseDC(m_hWnd, m_hDC);
            m_hDC = nullptr;
        }
    }

    void OpenglGrContext::Init()
    {
        if (!m_hDC) {
            XFATAL("Init 时 m_hDC 为空");
            return;
        }

        // 先创建一个兼容模式的 RC，保证能成功
        m_hGLRC = wglCreateContext(m_hDC);
        if (!m_hGLRC) {
            XFATAL("wglCreateContext 失败");
            return;
        }

        if (!MakeCurrent()) {
            XFATAL("MakeCurrent 失败");
            return;
        }

        XINFO("OpenGL 上下文初始化成功");
    }

    bool OpenglGrContext::MakeCurrent()
    {
        if (!m_hDC || !m_hGLRC) return false;
        return wglMakeCurrent(m_hDC, m_hGLRC) != FALSE;
    }

    void OpenglGrContext::SwapBuffers()
    {
        if (m_hDC) ::SwapBuffers(m_hDC);
    }

    bool OpenglGrContext::IsCurrent() const
    {
        return wglGetCurrentContext() == m_hGLRC;
    }

    void OpenglGrContext::DoneCurrent()
    {
        wglMakeCurrent(nullptr, nullptr);
    }

    void OpenglGrContext::SetupPixelFormat()
    {
        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,
            32,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0,
            24, 8, 0,
            PFD_MAIN_PLANE,
            0, 0, 0, 0
        };

        int fmt = ChoosePixelFormat(m_hDC, &pfd);
        if (!fmt) {
            XFATAL("ChoosePixelFormat 失败");
            return;
        }
        if (!SetPixelFormat(m_hDC, fmt, &pfd)) {
            XFATAL("SetPixelFormat 失败");
            return;
        }
    }

#endif
}