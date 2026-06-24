#include"xypch.h"
#include<Log/include/XYLog.h>
#include<XCore/include/XYTools.h>
//#include"Render/include/renderWin/renderWin.h"
//#include"Render/include/render.h"

#include"Application/include/Application.h"
#include"Window/include/XWidget.h"
//#include"GraphicsContext/include/GraphicsContext.h"
//#include"glad/glad.h"
void test() {
   XY_PROFILE_FUNCTION()
   return;
}
int main(int argc, char* argv[]) {
    X_Y::Application app(argc, argv);
    //std::cout << logger << std::endl;
    // ====================== 替换成你自己的 GLWin 窗口 ======================
    test();
    X_Y::XWidget win;
    win.setTitle("OpenGL 三角形");
    win.show();
    //win.createContext<X_Y::OpenglGrContext>();
    //X_Y::Render render;
    //win.RenderInit();
        while (app.isRunning()) {
            app.pushEvents();
            app.ProcessEvents();
    //    //    //win.Render();
    //        X_Y::RenderCommand::SetClearColor(X_Y::RenderMath::Vec4(1.0f, 0.75f, 0.8f, 1.0f));
    //        X_Y::RenderCommand::Clear();
    //        win.SwapBuffers();
 
        }
    while (1);
    return 0;
}