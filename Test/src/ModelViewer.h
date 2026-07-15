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

            // 切换 shader 后设置各自特有的 uniform
            m_Shader->Bind();
            m_Shader->SetFloat4("u_Color", m_Color);

            X_Y::Render::BeginScene(camera, view);
            X_Y::Render::Submit(m_Shader, m_BoxHandle, model);
            X_Y::Render::EndScene();
        }
    }

    bool loadBox()
    {
        std::string err;

        // 确保 box.obj 存在
        if (!X_Y::FilesSystem::FileExists("assets/box.obj"))
        {
            if (!X_Y::Model::GenerateBoxOBJ(1.0f, 1.0f, 1.0f, "assets/box.obj", err))
            {
                XERROR("GenerateBoxOBJ failed: {}", err);
                return false;
            }
        }

        // 走 OBJ 解析路径（诊断问题）
        X_Y::Model::Model model;
        if (!X_Y::Model::LoadObj("assets/box.obj", model, err))
        {
            XERROR("LoadObj failed: {}", err);
            return false;
        }

        XINFO("=== OBJ 解析诊断 ===");
        XINFO("顶点数: {} ({} pos floats)", model.vertices.size() / 3, model.vertices.size());
        XINFO("法线数: {} ({} normal floats)", model.normals.size() / 3, model.normals.size());
        XINFO("纹理数: {}", model.texcoords.size() / 2);
        XINFO("Mesh 数量: {}", model.meshes.size());
        for (size_t m = 0; m < model.meshes.size(); ++m)
        {
            auto& mesh = model.meshes[m];
            XINFO("  Mesh[{}] '{}': {} 个 Index 对象 ({} 个三角)",
                m, mesh.name.c_str(), mesh.indices.size(), mesh.indices.size() / 3);
            // 打印所有 Index 内容
            for (size_t i = 0; i < mesh.indices.size(); ++i) {
                auto& idx = mesh.indices[i];
                XINFO("    idx[{}]: v={} vt={} vn={}", i, idx.vertex, idx.texcoord, idx.normal);
            }
            //// 打印前 12 个 Index 内容（旧）
            //for (size_t i = 0; i < std::min(size_t(12), mesh.indices.size()); ++i) { (void)i; } // 占位
            //{
            //    auto& idx = mesh.indices[i];
            //    XINFO("    idx[{}]: v={} vt={} vn={}", i, idx.vertex, idx.texcoord, idx.normal);
            //}
        }

        if (model.meshes.empty())
        {
            XERROR("No meshes in box.obj");
            return false;
        }

        // 转成 MeshData
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

        XINFO("loadBox: handle={}, {} verts, {} indices ({} triangles)",
            m_BoxHandle,
            meshData.Vertices.size() / meshData.VertexStride,
            meshData.Indices.size(),
            meshData.Indices.size() / 3);

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

    X_Y::RenderMath::Vec4 m_Color = { 1.0f, 0.8f, 0.2f, 1.0f };  // 默认 FlatColor 颜色

    inline static X_Y::ShaderLibrary s_ShaderLib;
    inline static std::vector<std::string> s_ShaderNames;
};
