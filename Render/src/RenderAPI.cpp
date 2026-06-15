#include"Render/include/OpenGl/OpenGLRenderAPI.h"
#include"Log/include/XYLog.h"
namespace X_Y {
    Scope<RenderAPI> RenderAPI::Create(GraphicsType e) {
        XDEBUG("RenderAPI创建")
        switch (e) {
        case GraphicsType::None: XY_CORE_ASSERT(false, "API None not supported!"); return nullptr;
        case GraphicsType::OpenGL: return CreateScope<OpenGLRenderAPI>();
        }
        XY_CORE_ASSERT(false, "Unknown RenderAPI!");
        return nullptr;
    }
}