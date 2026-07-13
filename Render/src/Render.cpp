#include "Render.h"

namespace X_Y {
    Scope<Render::SceneData> Render::s_SceneData = CreateScope<Render::SceneData>();
    void Render::submitEvent(XMovement* e) {
        if (!e) return;
        XDEBUG("submit执行")

            RenderStart* p = dynamic_cast<RenderStart*>(e);
        if (!p)XFATAL("事件转化失败")

            GraphicsContext* GCT = p->m_GraphicsContext;
        if (!GCT)XFATAL("上下文是空指针")

            GraphicsType apiType = GCT->GetType();
        XDEBUG("当前API类型:{} ", apiType)

        auto it = m_RenderAPIs.find(apiType);
        if (it == m_RenderAPIs.end()) {
            XDEBUG("创建新 RenderAPI")
                auto api = RenderAPI::Create(apiType);
            api->Init();
            m_RenderAPIs[apiType] = std::move(api);
            it = m_RenderAPIs.find(apiType);
        }

        RenderAPI* currentAPI = it->second.get();
        m_LastSubmittedAPI = currentAPI; // 永远记录当前API

        // ==========================================
        // 2. 关键：不同窗口必须切换上下文！
        //   即使都是 OpenGL 也必须切换！
        // ==========================================
        if (!GCT->IsCurrent()) {
            XDEBUG("切换窗口上下文")
                GCT->MakeCurrent();
        }
    }
    void Render::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
    {
        shader->Bind();
        shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
        shader->SetMat4("u_Transform", transform);

        vertexArray->Bind();
        RenderCommand::DrawIndexed(vertexArray);
    }

    void Render::BeginScene(const Camera& camera, const RenderMath::Mat4& view)
    {
        s_SceneData->ViewProjectionMatrix = camera.GetProjection() * view;
    }
    void Render::EndScene()
    {
    }
    void Render::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }
}