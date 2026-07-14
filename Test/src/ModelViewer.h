#pragma once
#include"Render/include/renderWin/renderWin.h"
#include<Model/include/ModelLoader.h>
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

        // 挂 ImGuiLayer
        m_ImGuiLayer = new X_Y::ImGuiLayer(this->GetNativeWindow());
        X_Y::Application::instance()->PushLayer(m_ImGuiLayer);

        // 批量加载 assets/Shader/ 下所有 .glsl
        s_ShaderLib.LoadDirectory("assets/Shader");

        for (const auto& [name, _] : s_ShaderLib.GetAll())
            s_ShaderNames.push_back(name);

        if (!s_ShaderNames.empty())
            m_Shader = s_ShaderLib.Get(s_ShaderNames[0]);

        // 加载正方体
        loadBox();
    }

    ~ModelViewer()
    {
        if (m_BoxHandle >= 0)
            X_Y::Render::DestroyMesh(m_BoxHandle);

        if (m_ImGuiLayer)
            X_Y::Application::instance()->PopLayer(m_ImGuiLayer);
        delete m_ImGuiLayer;
    }

private:
    void onRender() override
    {
        X_Y::Render::SetClearColor(
            X_Y::RenderMath::Vec4(0.15f, 0.15f, 0.2f, 1.0f));
        X_Y::Render::Clear();

        // ── ImGui ──
        m_ImGuiLayer->Begin();
        {
            DrawShaderUI();
        }
        m_ImGuiLayer->End();

        // ── 绘制 3D ──
        if (m_Shader && m_BoxHandle >= 0)
        {
            // 简单透视相机
            X_Y::Camera camera(X_Y::RenderMath::Perspective(
                X_Y::RenderMath::Radians(45.0f),
                (float)get_width() / (float)get_height(),
                0.1f, 100.0f));

            X_Y::RenderMath::Mat4 view = X_Y::RenderMath::LookAt(
                X_Y::RenderMath::Vec3(3.0f, 3.0f, 5.0f),
                X_Y::RenderMath::Vec3(0.0f, 0.0f, 0.0f),
                X_Y::RenderMath::Vec3(0.0f, 1.0f, 0.0f));

            X_Y::RenderMath::Mat4 model = X_Y::RenderMath::Scale(
                X_Y::RenderMath::Mat4(1.0f),
                X_Y::RenderMath::Vec3(1.0f));

            X_Y::Render::BeginScene(camera, view);
            X_Y::Render::Submit(m_Shader, m_BoxHandle, model);
            X_Y::Render::EndScene();
        }
    }

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

        // 加载 .obj
        X_Y::Model::Model model;
        if (!X_Y::Model::LoadObj("assets/box.obj", model, err))
        {
            XERROR("LoadObj failed: {}", err);
            return false;
        }

        if (model.meshes.empty())
        {
            XERROR("No meshes in box.obj");
            return false;
        }

        // 转成通用 MeshData
        X_Y::MeshData meshData;
        if (!model.ConvertMeshToData(0, meshData, err))
        {
            XERROR("ConvertMeshToData failed: {}", err);
            return false;
        }

        // 交给 Render 创建 GPU 资源
        m_BoxHandle = X_Y::Render::CreateMesh(meshData);
        if (m_BoxHandle < 0)
        {
            XERROR("CreateMesh failed");
            return false;
        }

        XINFO("loadBox: handle={}, {} unique verts, {} indices",
            m_BoxHandle,
            meshData.Vertices.size() / meshData.VertexStride,
            meshData.Indices.size());
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
    int m_BoxHandle = -1;  ///< Render 返回的 GPU mesh handle
    inline static X_Y::ShaderLibrary s_ShaderLib;
    inline static std::vector<std::string> s_ShaderNames;
};
