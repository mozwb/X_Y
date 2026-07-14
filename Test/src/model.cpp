#include"xypch.h"
#include "Log/include/XYLog.h"
#include"ModelViewer.h"
int main(int argc, char* argv[])
{
    X_Y::Application app(argc, argv);

    ModelViewer win;
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