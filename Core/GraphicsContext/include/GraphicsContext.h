#pragma once
#include"GraphicsType.h"
#ifdef XY_PLATFORM_WINDOWS
#include <windows.h>
#endif
namespace X_Y {

	class GraphicsContext
	{
	protected:
		GraphicsType m_Type;
	public:
		virtual ~GraphicsContext() = default;
		virtual void Init() = 0;
		virtual bool MakeCurrent() = 0;
		virtual void SwapBuffers() = 0;
		virtual bool IsCurrent() const = 0;
		virtual void DoneCurrent() = 0;
		GraphicsType GetType() const { return m_Type; }
	};

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