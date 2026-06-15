#include"Render/include/VertexArray.h"
#include"Render/include/RenderAPI.h"
#include"Render/include/Render.h"
#include"Render/include/OpenGL/OpenGLVertexArray.h"
namespace X_Y {
	Ref<VertexArray> VertexArray::Create()
	{
		RApiType Api = Render::instance()->getCurrentAPI()->getType();
		switch (Api)
		{
		case RApiType::None:    XY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RApiType::OpenGL:  return CreateRef<OpenGLVertexArray>();
		}

		XY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}