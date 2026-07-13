#pragma once
#include"XCore/include/XYCore.h"
#include<unordered_map>
#include"Render/include/RenderCommand.h"
#include"Render/include/renderEvent/renderEvent.h"
#include"Render/include/VertexArray.h"
#include"Render/include/Shader.h"
#include"Render/include/Camera.h"
namespace X_Y {
    class Render
    {
    public:
        Render() = default;
        ~Render() = default;
        static Render* instance()
        {
            static Render* inst = nullptr;
            if (!inst)
                inst = new Render(); // 第一次调用才创建
            return inst;
        }
        void submitEvent( XMovement* e);
        RenderAPI* getCurrentAPI() {
            return m_LastSubmittedAPI;
        }

  
        static void Shutdown();
        static void OnWindowResize(uint32_t width, uint32_t height);

        static void BeginScene(const Camera& camera, const RenderMath::Mat4& view);
        static void EndScene();



        static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const RenderMath::Mat4& transform = glm::mat4(1.0f));
        Render(const Render&) = delete;
        Render& operator=(const Render&) = delete;
    private:
        struct SceneData
        {
            RenderMath::Mat4 ViewProjectionMatrix;
        };
        static Scope<SceneData> s_SceneData;
        RenderAPI* m_LastSubmittedAPI = nullptr;
        std::unordered_map<GraphicsType, Scope<RenderAPI>> m_RenderAPIs; 
    };
}