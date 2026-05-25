#pragma once
#include"Log/src/XYLog.h"
#include"Window/src/windows/BaseWin.h"
#include"window/src/movement/movements.h"
namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
    class Application
    {
    private:
        static Application* s_instance;
        MovementDispatcher m_dispatcher;
        MovementQueue m_eventQueue;
        bool Running = true;
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
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        void ProcessEvents() {
            while (!m_eventQueue.Empty()) {
                Movement* e = m_eventQueue.Pop();
                if (e) {
                    m_dispatcher.DispatchEvent(e);
                    auto* sender = (BaseWin*)e->sender;
                    std::string senderName;
                    if (sender) {
                        senderName = sender->toString();
                    }
                    if (e->Handled) {
                        XINFO("发起者:{} 事件{}：处理成功", senderName, e);
                        delete e;
                    } // 记得释放
                    else {
                        XINFO("发起者:{} 事件{}：处理失败", senderName, e);
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
    inline void connect(
        MovementSender sender,
        MovementType type,
        MovementReceiver receiver,
        MovementHandler handler
    ){
        Application* app = Application::instance();
        MovementDispatcher& dispatcher = app->GetDispatcher();
        dispatcher.Connect(sender, type, receiver, handler);
    }
    inline void disConnect(
        MovementSender sender,
        MovementType type,
        MovementReceiver receiver) {
        Application* app = Application::instance();
        MovementDispatcher& dispatcher = app->GetDispatcher();
        dispatcher.disConnect(sender, type, receiver);
    }
    inline void DisConnect(
        MovementReceiver receiver) {
        Application* app = Application::instance();
        MovementDispatcher& dispatcher = app->GetDispatcher();
        dispatcher.DisConnect(receiver);
    }
    template <
        typename SenderType,
        typename ReceiverType,
        typename... Args  // 任意个数参数
    >
    void connect(
        SenderType* sender,
        MovementType type,
        ReceiverType* receiver,
        void (ReceiverType::* slotFunc)(Args...)
    ) {
        // 自动包装成 lambda，你不用写！
        MovementHandler handler = [=]() {
            (receiver->*slotFunc)();
            };

        // 调用原来的 connect
        connect(sender, type, receiver, handler);
    }
#else
    ERROR("仅支持windows系统")
#endif
}