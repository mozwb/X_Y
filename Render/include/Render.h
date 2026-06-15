#pragma once
#include"XCore/include/XYCore.h"
#include<unordered_map>
#include"Render/include/RenderAPI.h"
#include"Render/include/renderEvent/renderEvent.h"
#include"Render/include/VertexArray.h"
#include"Render/include/Shader.h"
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

        static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const RenderMath::Mat4& transform = glm::mat4(1.0f));
        Render(const Render&) = delete;
        Render& operator=(const Render&) = delete;
    private:
        struct SceneData
        {
            RenderMath::Vec4 ViewProjectionMatrix;
        };
        RenderAPI* m_LastSubmittedAPI = nullptr;
        std::unordered_map<GraphicsType, Scope<RenderAPI>> m_RenderAPIs; 
    };
}