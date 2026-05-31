#pragma once
#include"XWidget.h"
namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
	class OpenglGrContext :public GraphicsContext {
	private:
		HGLRC m_hGLRC = nullptr;   // OpenGL 渲染上下文
		HDC m_hDC = nullptr;       // 设备上下文
		HWND m_hWnd = nullptr;
	public:
		OpenglGrContext(HWND hwnd);
		~OpenglGrContext() override;
		void Init()override;
		bool MakeCurrent()override;
		void SwapBuffers()override;
		bool IsCurrent()const override;
		void DoneCurrent()override;
		OpenglGrContext(const OpenglGrContext&) = delete;
		OpenglGrContext& operator=(const OpenglGrContext&) = delete;
		OpenglGrContext(OpenglGrContext&&) = delete;
		OpenglGrContext& operator=(OpenglGrContext&&) = delete;

	private:
		void SetupPixelFormat();
	};
#endif
}