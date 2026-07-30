#include "xypch.h"
#include "Log/include/XYLog.h"
#include "UI/include/dock/Docker.h"
#include "UI/include/Composite/LogViewer.h"
#include "DataStore/include/DataStore.h"


int main(int argc, char* argv[])
{
    X_Y::Application app(argc, argv);

    // ── 创建 Docker 主窗口 ──────────────────────────────
    auto* docker = new X_Y::Docker("Docker Test", 1280, 720);
    docker->show();

    // ── 日志查看器窗口 ──────────────────────────────────
    auto* logViewer = new X_Y::LogViewer();
    logViewer->show();
    logViewer->Start();

    // ── 窗口 2（备用） ──────────────────────────────────
    auto* win2 = new X_Y::XWidget();
    win2->setTitle("窗口 2 - 进制面板");
    win2->setSize(300, 200);
    win2->show();

    XINFO("LogViewer 测试启动 —— 日志将通过 DataStore 显示");

    // ── 事件循环 ────────────────────────────────────────
    while (app.isRunning())
    {
        app.ProcessEvents();
        app.pushEvents();
    }

    // ── 清理 ────────────────────────────────────────────
    //logViewer->Stop();
    //delete logViewer;
    //delete win2;
    //delete docker;

    logger.clear();
    // 保证 Buffer 在 Memory 析构前释放
    X_Y::DataStore::Instance().ClearAll();

    return 0;
}
