#pragma once
#include"Render/include/renderWin/renderWin.h"
#include<Model/include/ModelLoader.h>
#include<Model/include/ModelToGPU.h>
#include<Model/include/ModelGenerator.h>
#include<UI/include/imgui/ImGuiLayer.h>
#include<FilesSystem/include/FilesSystem.h>
class ModelViewer : public X_Y::RenderWin {
public:
    ModelViewer()
        : X_Y::RenderWin(nullptr)
    {
        setTitle("Model Viewer");
        setSize(1280, 720);
    }

    void setup()
    {
        createContext<X_Y::OpenglGrContext>();
        this->RenderInit();

        // 挂 ImGuiLayer（OnAttach 完成 ImGui 初始化）
        m_ImGuiLayer = new X_Y::ImGuiLayer(this->GetNativeWindow());
        X_Y::Application::instance()->PushLayer(m_ImGuiLayer);

        // 批量加载 assets/Shader/ 下所有 .glsl，按文件名命名
        s_ShaderLib.LoadDirectory("assets/Shader");

        // 填充 shader 名称列表
        for (const auto& [name, _] : s_ShaderLib.GetAll())
            s_ShaderNames.push_back(name);

        // 默认用第一个
        if (!s_ShaderNames.empty())
            m_Shader = s_ShaderLib.Get(s_ShaderNames[0]);
    }

    ~ModelViewer()
    {
        if (m_ImGuiLayer)
            X_Y::Application::instance()->PopLayer(m_ImGuiLayer);
		delete m_ImGuiLayer;
    }

private:
    void onRender() override
    {
        // 3D 渲染
        X_Y::RenderCommand::SetClearColor(
            X_Y::RenderMath::Vec4(0.15f, 0.15f, 0.2f, 1.0f));
        X_Y::RenderCommand::Clear();

        // ── ImGui ──
        m_ImGuiLayer->Begin();
        {
            DrawShaderUI();
        }
        m_ImGuiLayer->End();

        // ── 绘制 3D ──
        if (m_Shader)
        {
            m_Shader->Bind();
            // 后续绘制调用放在这里
        }
    }
    //上传基本的obj数据
	//如果没有box.obj文件，则生成一个
    //有就直接加载
    bool loadBox()
    {
        std::string err;

        // 没有才生成
        if (!X_Y::FilesSystem::FileExists("assets/box.obj"))
        {
            if (!X_Y::Model::GenerateBoxOBJ(1.0f, 1.0f, 1.0f, "assets/box.obj", err))
            {
                XERROR("GenerateBoxOBJ failed: {}", err);
                return false;
            }
        }

        if (!X_Y::Model::LoadAndPrepare("assets/box.obj", m_Model, err))
        {
            XERROR("LoadAndPrepare failed: {}", err);
            return false;
        }

        return true;
    }
    void DrawShaderUI()
    {
        ImGui::Begin("Shader Selector");
        ImGui::Text("Cached Shaders:");
        ImGui::Separator();

        for (size_t i = 0; i < s_ShaderNames.size(); ++i)
        {
            bool selected = (m_Shader && m_Shader->GetName() == s_ShaderNames[i]);
            if (ImGui::Selectable(s_ShaderNames[i].c_str(), selected))
            {
                m_Shader = s_ShaderLib.Get(s_ShaderNames[i]);
            }
        }

        ImGui::End();
    }

    X_Y::ImGuiLayer* m_ImGuiLayer = nullptr;
    X_Y::Ref<X_Y::Shader> m_Shader;
	X_Y::Model::GPUModel m_Model;
    inline static X_Y::ShaderLibrary s_ShaderLib;
    inline static std::vector<std::string> s_ShaderNames;
};
