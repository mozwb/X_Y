#pragma once
#include "vendor/imgui/imgui.h"
#include"vendor/imgui/backends/imgui_impl_win32.h"
#include"vendor/imgui/backends/imgui_impl_opengl3.h"
#include"Window/include/XWidget.h"
namespace X_Y {
namespace ImGuiBuild {

	// 初始化 ImGui 上下文 + Win32/OpenGL3 backend
	// hwnd：窗口句柄，传 GetNativeWindow()
	// glslVersion：OpenGL GLSL 版本号字符串，如 "#version 330"
	inline void Init(void* hwnd, const char* glslVersion = "#version 330")
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		ImGui_ImplWin32_InitForOpenGL(hwnd);
		ImGui_ImplOpenGL3_Init(glslVersion);
	}

	// 每帧开头调用：开始 ImGui 帧
	inline void NewFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	// 每帧 Render 调用：渲染 ImGui 绘制数据
	inline void Render()
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	// 程序退出时调用：清理 ImGui
	inline void Shutdown()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

} // namespace ImGuiBuild
} // namespace X_Y
