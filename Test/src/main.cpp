#include"xypch.h"
#include<Log/include/XYLog.h>
#include<XCore/include/XYTools.h>
#include<Application/include/Application.h>
#include<Render/include/renderWin/renderWin.h>
#include<Render/include/RenderCommand.h>
#include<Render/include/renderMath/RenderMath.h>
#include<Model/include/ModelLoader.h>
#include<Model/include/ModelToGPU.h>
#include<glad/glad.h>

// ── Shader ──
static const char* s_ModelVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec3 v_Normal;
out vec3 v_Position;

void main()
{
    vec4 worldPos = u_Transform * vec4(a_Position, 1.0);
    gl_Position = u_ViewProjection * worldPos;
    v_Normal   = normalize(mat3(u_Transform) * a_Normal);
    v_Position = worldPos.xyz;
}
)";

static const char* s_ModelFragmentSrc = R"(
#version 330 core
in vec3 v_Normal;
in vec3 v_Position;

out vec4 FragColor;

void main()
{
    // 用法线作为颜色调试：normal 范围 [-1,1] → 颜色 [0,1]
    vec3 debugColor = normalize(v_Normal) * 0.5 + 0.5;
    FragColor = vec4(debugColor, 1.0);
}
)";

// ── ModelViewerWin ──
class ModelViewerWin : public X_Y::RenderWin
{
public:
    ModelViewerWin()
        : X_Y::RenderWin(nullptr)
    {
        setTitle("Model Viewer");
        setSize(1280, 720);
    }

    void setup()
    {
        createContext<X_Y::OpenglGrContext>();

        X_Y::RenderStart* e = new X_Y::RenderStart(this,
            get_context(), get_width(), get_height());
        X_Y::Application::instance()->GetEventQueue().Push(e);
        X_Y::Application::instance()->ProcessEvents();

        m_Shader = X_Y::Shader::Create("ModelShader",
            s_ModelVertexSrc, s_ModelFragmentSrc);

        LoadModel("assets/teapot.obj");
    }

    struct MeshGPU {
        X_Y::Ref<X_Y::VertexArray> VAO;
        uint32_t IndexCount;
    };

    void LoadModel(const std::string& filepath)
    {
        X_Y::Model::Model model;
        std::string err;

        if (!X_Y::Model::LoadObj(filepath, model, err))
        {
            XERROR("Failed to load model '{0}': {1}", filepath, err);
            return;
        }

        XINFO("Loaded '{0}': {1} vertices, {2} normals, {3} texcoords, {4} meshes",
            filepath,
            model.vertices.size() / 3,
            model.normals.size() / 3,
            model.texcoords.size() / 2,
            model.meshes.size());

        // debug: 打印第一个 mesh 的前几个 index
        if (!model.meshes.empty())
        {
            const auto& first = model.meshes[0];
            size_t show = std::min<size_t>(first.indices.size(), 12);
            XINFO("First mesh '{0}' first {1} indices:",
                first.name, show);
            for (size_t i = 0; i < show; ++i)
            {
                auto& idx = first.indices[i];
                XINFO("  [{0}] v={1} vt={2} vn={3}",
                    i, idx.vertex, idx.texcoord, idx.normal);
            }
        }

        m_Meshes.reserve(model.meshes.size());

        for (const auto& mesh : model.meshes)
        {
            X_Y::Model::GPUMeshData gpuData;
            std::string prepareErr;

            if (!X_Y::Model::PrepareMesh(model, mesh, gpuData, prepareErr))
            {
                XERROR("PrepareMesh failed for '{0}': {1}", mesh.name, prepareErr);
                continue;
            }

            // debug: 打印前几个 GPU 顶点
            if (!gpuData.Vertices.empty())
            {
                size_t show = std::min<size_t>(gpuData.Vertices.size() / X_Y::Model::kVertexStride, 3);
                XINFO("GPU vertex debug (first {0}):", show);
                for (size_t i = 0; i < show; ++i)
                {
                    size_t off = i * X_Y::Model::kVertexStride;
                    XINFO("  [{0}] pos=({1:.3f},{2:.3f},{3:.3f}) nrm=({4:.3f},{5:.3f},{6:.3f})",
                        i,
                        gpuData.Vertices[off + 0], gpuData.Vertices[off + 1], gpuData.Vertices[off + 2],
                        gpuData.Vertices[off + 3], gpuData.Vertices[off + 4], gpuData.Vertices[off + 5]);
                }
            }

            auto va = X_Y::VertexArray::Create();

            auto vb = X_Y::VertexBuffer::Create(
                gpuData.Vertices.data(),
                static_cast<uint32_t>(gpuData.Vertices.size() * sizeof(float)));
            vb->SetLayout({
                { X_Y::ShaderDataType::Float3, "a_Position" },
                { X_Y::ShaderDataType::Float3, "a_Normal"   }
            });
            va->AddVertexBuffer(vb);

            auto ib = X_Y::IndexBuffer::Create(
                gpuData.Indices.data(),
                static_cast<uint32_t>(gpuData.Indices.size()));
            va->SetIndexBuffer(ib);

            m_Meshes.push_back({ va, static_cast<uint32_t>(gpuData.Indices.size()) });
            XINFO("  Mesh '{0}': {1} unique vertices, {2} indices",
                mesh.name,
                gpuData.Vertices.size() / X_Y::Model::kVertexStride,
                gpuData.Indices.size());
        }

        if (!m_Meshes.empty())
            XINFO("✅ Model '{0}' uploaded to GPU, {1} meshes", filepath, m_Meshes.size());
    }

    ~ModelViewerWin() = default;

protected:
    void onRender() override
    {
        X_Y::RenderCommand::SetClearColor(
            X_Y::RenderMath::Vec4(0.15f, 0.15f, 0.2f, 1.0f));
        X_Y::RenderCommand::Clear();

        if (m_Shader && !m_Meshes.empty())
        {
            m_Shader->Bind();

            X_Y::RenderMath::Mat4 view = X_Y::RenderMath::LookAt(
                X_Y::RenderMath::Vec3(3.0f, 3.0f, 5.0f),
                X_Y::RenderMath::Vec3(0.0f, 0.0f, 0.0f),
                X_Y::RenderMath::Vec3(0.0f, 1.0f, 0.0f)
            );

            float aspect = static_cast<float>(get_width()) / get_height();
            X_Y::RenderMath::Mat4 proj = X_Y::RenderMath::Perspective(
                X_Y::RenderMath::Radians(45.0f), aspect, 0.1f, 100.0f);

            X_Y::RenderMath::Mat4 vp = proj * view;

            // 茶壶范围 ~40，缩放到 1/40 倍
            X_Y::RenderMath::Mat4 model = X_Y::RenderMath::Scale(
                X_Y::RenderMath::Mat4(1.0f),
                X_Y::RenderMath::Vec3(0.025f));
            // 茶壶顶点在 0~40 范围，平移使中心到原点
            model = X_Y::RenderMath::Translate(model,
                X_Y::RenderMath::Vec3(-20.0f, -15.0f, 0.0f));

            m_Shader->SetMat4("u_ViewProjection", vp);
            m_Shader->SetMat4("u_Transform", model);

            for (const auto& mesh : m_Meshes)
            {
                X_Y::RenderCommand::DrawIndexed(mesh.VAO, mesh.IndexCount);
            }
        }
    }

private:
    X_Y::Ref<X_Y::Shader> m_Shader;
    std::vector<MeshGPU> m_Meshes;
};

int main(int argc, char* argv[])
{
    X_Y::Application app(argc, argv);

    ModelViewerWin win;
    win.show();
    win.setup();

    while (app.isRunning())
    {
        app.ProcessEvents();
        win.Render();
        app.pushEvents();
    }

    return 0;
}
