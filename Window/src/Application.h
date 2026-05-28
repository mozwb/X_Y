#pragma once
#include"Log/src/XYLog.h"
#include"Window/src/windows/BaseWin.h"
#include"window/src/movement/movements.h"
namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
    
    //这个管理了程序运行周期，接受win32消息转化系统事件，可以仿照这个写渲染层或者其他层
    //总线接受所有事件，只负责系统事件，其他再下发执行
    class Application
    {
    private:
        static Application* s_instance;
        MovementDispatcher m_dispatcher;
        MovementQueue m_eventQueue;
        bool Running = true;
         bool FirstWin = false;
    public:
        Application(int argc, char* argv[]);
        ~Application();
        // 消息循环
        void exec();
        void pushEvents();
        void ProcessEvents();
        bool isRunning() { return Running; }
        void appClose() {
            Running = false;
            FirstWin = false;
        }
        MovementDispatcher& GetDispatcher() { return m_dispatcher; }
        MovementQueue& GetEventQueue() { return m_eventQueue; }
        static Application* instance()
        {
            return s_instance;
        }
        bool IsFirstWin() {
            return FirstWin;
        }
        void updateFirstWin() {
            FirstWin = true;
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
    inline void disConnect(
        MovementSender sender, 
        MovementReceiver receiver
        ) {
        Application* app = Application::instance();
        MovementDispatcher& dispatcher = app->GetDispatcher();
        dispatcher.disConnect(sender,receiver);
    }
    inline void disConnect(
        MovementReceiver receiver) {
        Application* app = Application::instance();
        MovementDispatcher& dispatcher = app->GetDispatcher();
        dispatcher.disConnect(receiver);
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