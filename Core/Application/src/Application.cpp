#include"Application.h"
//#include"Window/include/BaseWin.h"
namespace X_Y {
    Application::Application(int argc, char* argv[])
    {
        s_instance = this;
    }
    Application::~Application()
    {
        s_instance = nullptr;
    }

    // 消息循环
    void Application::exec()
    {
        while (Running)
        {
            pushEvents();
            ProcessEvents();
        }
    }
    void Application::pushEvents() {
        MSG msg{};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    void Application::pushEvents(XMovement* e) {
        m_eventQueue.Push(e);
    }
    void Application::ProcessEvents() {
        while (!m_eventQueue.Empty()) {
            X_Y::XMovement* baseEvt = m_eventQueue.Pop();
            if (!baseEvt) continue;
            baseEvt->DispatchEvent(static_cast<void*>(&m_dispatcher));
            delete baseEvt;
        }
    }

}