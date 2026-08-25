#pragma once
#include "Log/XYLog.h"
#include "Movement/Movements.h"
#include <functional>
#include <memory>
namespace X_Y
{

    // 这个管理了程序运行周期，接受win32消息转化系统事件，可以仿照这个写渲染层或者其他层
    // 总线接受所有事件，只负责系统事件，其他再下发执行
    class Application
    {
        using MouseRedirectHandler = std::function<bool(const XMovement &)>;

    private:
        inline static Application *s_instance = nullptr; // 确保全局唯一并在main前初始化，main后析构
        MovementDispatcher m_dispatcher;
        MovementQueue m_eventQueue;
        bool Running = true;
        bool FirstWin = false;
        // 如果你需要打包一些逻辑，你可以写层栈，从而减轻事件分发器压力
        LayerStack m_LayerStack;
        std::unique_ptr<class PlatformLoop> m_PlatformLoop;
        MouseRedirectHandler m_MouseRedirectHandler;

    public:
        // 1.构造、析构放protected，允许子类继承构造，禁止外部new
        Application(int argc, char *argv[]);

        virtual ~Application(); // 2.虚析构，多态析构必备
        // 消息循环
        virtual void exec();
        virtual void pushEvents();
        virtual void pushEvents(XMovement *e);
        virtual void ProcessEvents();
        bool isRunning() { return Running; }
        void appClose()
        {
            Running = false;
            FirstWin = false;
        }
        MovementDispatcher &GetDispatcher() { return m_dispatcher; }
        MovementQueue &GetEventQueue() { return m_eventQueue; }

        // ── 全局鼠标事件钩子（命中重定向，方案A预埋） ──────────────
        // 单一占用式的鼠标级拦截点，供 DockLayout 等"事件源头重定向"使用。
        // 背景：Win32 下鼠标点在最顶层覆盖的子窗口上，sender=子窗口；
        //       Dispatcher 按 sender 精确匹配、不冒泡，父窗口(DockLayout)收不到
        //       Dock 区域的按下/移动。Connect 无法解决"吃事件"。
        //       ⇒ 在派发前加一个全局鼠标钩子：进程每出队一个事件先交给它，
        //         命中(如分割线)则由钩子自己驱动并返回 true 吞掉原事件，
        //         未命中返回 false，事件照常走 dispatcher。
        // 语义：钩子接收每个待派发事件；返回 true = 已处理，跳过 dispatcher。
        void SetMouseRedirectHandler(MouseRedirectHandler handler)
        {
            m_MouseRedirectHandler = std::move(handler);
        }
        void ClearMouseRedirectHandler()
        {
            m_MouseRedirectHandler = nullptr;
        }
        bool HasMouseRedirectHandler() const { return (bool)m_MouseRedirectHandler; }
        static Application *instance()
        {
            return s_instance;
        }
        bool IsFirstWin()
        {
            return FirstWin;
        }
        void updateFirstWin()
        {
            FirstWin = true;
        }
        void PushLayer(Layer *layer)
        {
            m_LayerStack.PushLayer(layer);
        }
        void PopLayer(Layer *layer)
        {
            m_LayerStack.PopLayer(layer);
        }

        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;
    };

    template <typename EnumT>
    void Connect(
        MovementSender sender,
        EnumT type, // 直接保留原始枚举！
        MovementReceiver receiver,
        MovementHandler handler)
    {
        auto &dispatcher = Application::instance()->GetDispatcher();
        // 直接把 EnumT 传给 dispatcher，类型 100% 保留！
        dispatcher.Connect<EnumT>(sender, type, receiver, std::move(handler));
    }
    template <typename EnumT>
    void disConnect(
        MovementSender sender,
        EnumT type,
        MovementReceiver receiver)
    {
        auto &dispatcher = Application::instance()->GetDispatcher();
        dispatcher.disConnect<EnumT>(sender, type, receiver);
    }
    inline void disConnect(
        MovementSender sender,
        MovementReceiver receiver)
    {
        Application *app = Application::instance();
        MovementDispatcher &dispatcher = app->GetDispatcher();
        dispatcher.disConnect(sender, receiver);
    }
    inline void disConnect(
        MovementReceiver receiver)
    {
        Application *app = Application::instance();
        MovementDispatcher &dispatcher = app->GetDispatcher();
        dispatcher.disConnect(receiver);
    }
    template <typename S, typename E, typename R>
    void connect(
        S *sender,
        E type,
        R *receiver,
        void (R::*slotFunc)(const XMovement &))
    {
        MovementHandler handler = [=](const XMovement &e)
        {
            (receiver->*slotFunc)(e);
        };
        Connect(sender, type, receiver, std::move(handler));
    }
    template <typename S, typename E, typename R>
    void connect(
        S *sender,
        E type,
        R *receiver,
        void (R::*slotFunc)(XMovement *))
    {
        MovementHandler handler = [=](const XMovement &e)
        {
            (receiver->*slotFunc)(const_cast<XMovement *>(&e));
        };
        Connect(sender, type, receiver, std::move(handler));
    }
    template <typename S, typename E, typename R>
    void connect(
        S *sender,
        E type,
        R *receiver,
        void (R::*slotFunc)())
    {
        MovementHandler handler = [=](const XMovement &)
        {
            (receiver->*slotFunc)();
        };
        Connect(sender, type, receiver, std::move(handler));
    }

    /*摘自Copilot的总结：
    •	Connect（大写）
•	模板定义：template<typename EnumT> void Connect(MovementSender sender, EnumT type, MovementReceiver receiver, MovementHandler handler)
•	最底层直接走 MovementDispatcherConnect<EnumT>(...)，接收一个 MovementHandler（即 stdfunction<void(const XMovement&)>）。
•	适合注册任意可调用对象（lambda、std::bind、函数对象、静态函数等）。
•	注册时你需要自己写 handler 的参数和转换（例如 const XMovement&），并且传入 receiver（通常为 this）以便后续 disconnect。
•	connect（小写）
•	是对 Connect 的便利封装，提供了多个重载来接受成员函数指针：
•	void (R::* slotFunc)(const XMovement&)
•	void (R::* slotFunc)(XMovement*)
•	void (R::* slotFunc)()
•	内部把成员函数包装成 MovementHandler（通过 lambda 捕获 receiver 并在回调中调用成员函数），然后调用 Connect(...)。
•	方便把某对象的成员函数直接绑定为回调，不需要自己写 MovementHandler。
关键影响
•	如果要传入带捕获的 lambda（如 [this](Movement& e){...}），现有的小写 connect 没有接受普通 lambda 的重载，需要用大写 Connect 并传入 MovementHandler（并把参数类型改为 const XMovement&）。
•	使用小写 connect 时，disconnect 可以通过传入 receiver 指针方便地解绑对应绑定（因为封装里把 receiver 传给 Connect）。用大写 Connect 同样可以把 receiver 传入，但要注意 handler 的捕获不要导致悬 dangling 指针。
建议
•	绑定成员函数：用小写 connect（更简洁、类型安全）。
•	绑定 lambda / 自定义函数对象：用大写 Connect（传 MovementHandler），并确保 handler 参数类型为 const XMovement&（或按 dispatcher 期望的类型）

    */
}
