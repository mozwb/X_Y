#pragma once
#include"XCore/include/XYCore.h"
#include<unordered_map>
#include"Render/include/RenderAPI/RenderAPI.h"
#include"Render/include/renderEvent/renderEvent.h"
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
        void submit( XMovement* e);

        RenderAPI* getCurrentAPI() {
            return m_LastSubmittedAPI;
        }


        Render(const Render&) = delete;
        Render& operator=(const Render&) = delete;
    private:

        RenderAPI* m_LastSubmittedAPI = nullptr;
        std::unordered_map<GraphicsType, Scope<RenderAPI>> m_RenderAPIs; 
    };
}