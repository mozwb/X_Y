#include"xypch.h"
#include<Log/include/XYLog.h>
#include"Render/include/renderWin/renderWin.h"
#include"Render/include/render.h"
#include"Application/include/Application.h"
#include"GraphicsContext/include/GraphicsContext.h"
#include"glad/glad.h"
int main(int argc, char* argv[]) {
    X_Y::Application app(argc, argv);

    // ====================== 替换成你自己的 GLWin 窗口 ======================
    X_Y::RenderWin win;
    win.setTitle("OpenGL 三角形");
    win.show();
    win.createContext<X_Y::OpenglGrContext>();
    X_Y::Render render;
    win.RenderInit();
        //while (app.isRunning()) {
        //    app.pushEvents();
        //    app.ProcessEvents();
        ////    //win.Render();
        //    X_Y::RenderCommand::SetClearColor(X_Y::RenderMath::Vec4(1.0f, 0.75f, 0.8f, 1.0f));
        //    X_Y::RenderCommand::Clear();
        //    win.get_context()->SwapBuffers();
        //    //while (true);
        //}
        while (true);


    return 0;
}