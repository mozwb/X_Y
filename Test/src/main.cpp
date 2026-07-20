#include "xypch.h"
#include "Log/include/XYLog.h"
#include "UI/include/dock/Docker.h"


int main(int argc, char* argv[])
{
    X_Y::Application app(argc, argv);

    // ── 创建 Docker 主窗口 ──────────────────────────────
    auto* docker = new X_Y::Docker("Docker Test", 1280, 720);
    docker->show();

    // ── 创建两个独立窗口（将拖入 Docker） ──────────────
    auto* win1 = new X_Y::XWidget();
    win1->setTitle("窗口 1 - 日志面板");
    win1->setSize(300, 200);
    win1->show();

    auto* win2 = new X_Y::XWidget();
    win2->setTitle("窗口 2 - 进制面板");
    win2->setSize(300, 200);
    win2->show();

    XINFO("Docker 测试启动 —— 拖拽窗口1/2的标题栏到Docker窗口查看停靠效果");

    // ── 事件循环 ────────────────────────────────────────
    while (app.isRunning())
    {
        app.ProcessEvents();
        app.pushEvents();
    }

    return 0;
}
