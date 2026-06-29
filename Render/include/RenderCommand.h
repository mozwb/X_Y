#pragma once
#include"Render/include/RenderAPI.h"
#include"render.h"
namespace X_Y {
       class RenderCommand
        {
        public:
            // 所有接口 自动获取当前激活的API
            static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
            {
                Render::instance()->getCurrentAPI()->SetViewport(x, y, width, height);
            }

            static void SetClearColor(const RenderMath::Vec4& color)
            {
                Render::instance()->getCurrentAPI()->SetClearColor(color);
            }

            static void Clear()
            {
                Render::instance()->getCurrentAPI()->Clear();
            }
            static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0)
            {
                Render::instance()->getCurrentAPI()->DrawIndexed(vertexArray, indexCount);
            }

            static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
            {
                Render::instance()->getCurrentAPI()->DrawLines(vertexArray, vertexCount);
            }


            static void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
            {
                Render::instance()->getCurrentAPI()->DrawArrays(vertexArray, vertexCount);
            }
	private:
		static RenderAPI* s_RendererAPI;
	};

}