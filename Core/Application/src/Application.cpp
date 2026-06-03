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
    void Application::ProcessEvents() {
        while (!m_eventQueue.Empty()) {
            Movement* e = m_eventQueue.Pop();
            if (e) {
                m_dispatcher.DispatchEvent(e);
                //auto* sender = (BaseWin*)e->sender;
                //std::string senderName;
                //if (sender) {
                //    senderName = sender->toString();
                //}
                if (e->Handled) {
                    if(e->GetType()!=X_Y::MovementType::WindowPaint)
                    XINFO(" 事件{}：处理成功", e);
                    delete e;
                } // 记得释放
                else {
                    XINFO(" 事件{}：处理失败", e);
                    delete(e);
                }
            }
        }
    }

}