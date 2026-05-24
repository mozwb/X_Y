#pragma once
#include"Log/src/XYLog.h"
#include"Window/src/movement/movements.h"
#ifdef XY_PLATFORM_WINDOWS
#include"Window/src/windows/WinCore.h"
namespace X_Y {
    class Application
    {
    private:
        static Application* s_instance;
        MovementDispatcher m_dispatcher;
        MovementQueue m_eventQueue;
        bool Running=true;
    public:
        Application(int argc, char* argv[])
        {
            s_instance = this;
        }
        ~Application()
        {
            s_instance = nullptr;
        }

        // 消息循环
        void exec()
        {
            while (Running)
            {
                pushEvents();
                ProcessEvents();
            }
        }
        void pushEvents() {
            MSG msg{};
            if (PeekMessage(&msg, NULL, 0, 0,PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        void ProcessEvents() {
            while (!m_eventQueue.Empty()) {
                Movement* e = m_eventQueue.Pop();
                if (e) {
                    m_dispatcher.DispatchEvent(e);
                    if (e->Handled) { 
                        XINFO("{}：处理成功",e)
                        delete e; } // 记得释放
                    else {
                        XERROR("{}：处理失败", e)
                        delete(e);
                    }
                }
            }
        }
        bool isRunning() { return Running; }
        void appClose() { Running = false; }
        MovementDispatcher& GetDispatcher() { return m_dispatcher; }
        MovementQueue& GetEventQueue() { return m_eventQueue; }
        static Application* instance()
        {
            return s_instance;
        }
    };
    inline Application* Application::s_instance = nullptr;
}
#else
    ERROR("仅支持windows系统")
#endif