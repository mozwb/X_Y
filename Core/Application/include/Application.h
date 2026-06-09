#pragma once
#include"Log/include/XYLog.h"
#include"Movement/include/Movements.h"
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
        //如果你需要打包一些逻辑，你可以写层栈，从而减轻事件分发器压力
        LayerStack m_LayerStack;  

    public:
        // 1.构造、析构放protected，允许子类继承构造，禁止外部new
        Application(int argc, char* argv[]);
        virtual ~Application();// 2.虚析构，多态析构必备
        // 消息循环
        virtual void exec();
        virtual void pushEvents();
        virtual void pushEvents(XMovement* e);
        virtual void ProcessEvents();
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
        void PushLayer(Layer* layer) {
            m_LayerStack.PushLayer(layer);
        }
        void PopLayer(Layer* layer) {
            m_LayerStack.PopLayer(layer);
                    }

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
    };
    inline Application* Application::s_instance = nullptr;
 
    template <typename EnumT>
    void Connect(
        MovementSender sender,
        EnumT type,            // 直接保留原始枚举！
        MovementReceiver receiver,
        MovementHandler handler
    ) {
        auto& dispatcher = Application::instance()->GetDispatcher();
        // 直接把 EnumT 传给 dispatcher，类型 100% 保留！
        dispatcher.Connect<EnumT>(sender, type, receiver, std::move(handler));
    }
    template <typename EnumT>
    void disConnect(
        MovementSender sender,
        EnumT type,
        MovementReceiver receiver
    ) {
        auto& dispatcher = Application::instance()->GetDispatcher();
        dispatcher.disConnect<EnumT>(sender, type, receiver);
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
    template <typename S, typename E, typename R>
    void connect(
        S* sender,
        E type,
        R* receiver,
        void (R::* slotFunc)(const XMovement&)
    ) {
        MovementHandler handler = [=](const XMovement& e) {
            (receiver->*slotFunc)(e);
            };
        Connect(sender, type, receiver, std::move(handler));
    }
    template <typename S, typename E, typename R>
    void connect(
        S* sender,
        E type,
        R* receiver,
        void (R::* slotFunc)(XMovement*)
    ) {
        MovementHandler handler = [=](const XMovement& e) {
            (receiver->*slotFunc)(const_cast<XMovement*>(&e));
            };
        Connect(sender, type, receiver, std::move(handler));
    }
    template <typename S, typename E, typename R>
    void connect(
        S* sender,
        E type,
        R* receiver,
        void (R::* slotFunc)()
    ) {
        MovementHandler handler = [=](const XMovement&) {
            (receiver->*slotFunc)();
            };
        Connect(sender, type, receiver, std::move(handler));
    }
#else
    ERROR("仅支持windows系统")
#endif
}