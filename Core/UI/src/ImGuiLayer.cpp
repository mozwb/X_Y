#include<imgui/ImGuiLayer.h>
#include<imgui_internal.h>
#include<Window/include/WinCore.h>

// ImGui_ImplWin32_WndProcHandler 声明在头文件中被 #if 0 禁用，需手动 extern
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace X_Y {

	// 注册 WndProc hook：让 Window 模块把消息先给 ImGui 处理
	static void RegisterWndProcHook()
	{
		WinCore::g_WndProcHook = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
			if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
				return true;
			return false;
		};
	}

	ImGuiLayer::ImGuiLayer()
		: Layer()
	{
		RegisterWndProcHook();
	}

	ImGuiLayer::ImGuiLayer(void* hwnd)
		: Layer(), m_Hwnd(hwnd)
	{
		RegisterWndProcHook();
	}

	void ImGuiLayer::OnAttach()
	{
		// 在此完成 ImGui 初始化，不用外部调 ImGuiBuild::Init
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		SetDarkThemeColors();

		ImGui_ImplWin32_InitForOpenGL(m_Hwnd);
		XINFO("ImGui 已初始化");
		ImGui_ImplOpenGL3_Init("#version 330");
	}

	void ImGuiLayer::OnDetach()
	{
		// 在此清理，不用外部调 ImGuiBuild::Shutdown
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		XINFO("ImGui 已清理");
	}

	void ImGuiLayer::OnEvent(XMovement* e)
	{
		if (!m_BlockEvents)
			return;

		//// 只有键鼠事件才可能被 ImGui 拦截，窗口关闭/缩放等不拦截
		//if (auto* movement = dynamic_cast<Movement*>(e))
		//{
		//	if (movement->IsInCategory(MTMouse) || movement->IsInCategory(MTKeyboard))
		//	{
		//		ImGuiIO& io = ImGui::GetIO();
		//		e->Handled |= io.WantCaptureMouse;
		//		e->Handled |= io.WantCaptureKeyboard;
		//	}
		//}
	}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::End()
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		ImGui::StyleColorsDark();
	}

	uint32_t ImGuiLayer::GetActiveWidgetID() const
	{
		return ImGui::GetActiveID();
	}

}
