#pragma once
#include "Log/XYLog.h"
#include "Movement/Movements.h"
#include <functional>
#include <memory>
#include <vector>
namespace X_Y
{

    class XWidget; // 主窗口类型（仅需指针，前置声明即可）

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

        // ── 主窗口（退出语义）──
        // 【手动指定】"关掉它就退出程序"的窗口。
        // 取代旧的「进程里第一个建的顶层窗口自动当首窗」—— 那种约定会被
        // 瞬时窗口（自检探针窗、临时窗）抢走名额，导致没人负责退出。
        // 未指定（nullptr）时：关闭任何窗口都只关它自己，程序不退出。
        XWidget *m_MainWindow = nullptr;

        // 主窗口若走原生销毁路径（非 WindowClose）→ 清引用，防悬空。
        void OnMainWindowDestroyed() { m_MainWindow = nullptr; }

        // ── 托管窗口名单 ──
        // 手动把窗口交给 Application 托管（Own）。程序退出时（Shutdown）名单里
        // 还活着的窗口会被 destroy + 回收 —— 避免退出后残留（析构不跑、
        // 线程不停、资源泄漏）。窗口正常销毁时会自动从名单摘除。
        std::vector<XWidget *> m_OwnedWindows;
        static constexpr int kMaxShutdownRounds = 100000; // 兜底防死循环
        // 如果你需要打包一些逻辑，你可以写层栈，从而减轻事件分发器压力
        LayerStack m_LayerStack;
        std::unique_ptr<class PlatformLoop> m_PlatformLoop;
        MouseRedirectHandler m_MouseRedirectHandler;

        // ============================================================
        // 延迟回收队列 —— 通用资源回收（不限于窗口）
        //
        // 存的是【回收动作】而不是裸指针：
        //   void* 会丢掉类型 —— 多态对象（Container 等）用基类指针 delete
        //   走不到派生析构，是 UB + 泄漏。让每个入队者自己写"怎么释放"，
        //   通用性就落在"怎么回收"这半上：
        //     窗口  → [this]{ delete this; }
        //     句柄  → [h]{ ::CloseHandle(h); }
        //     GL 对象 → [t]{ glDeleteTextures(1, &t); }
        //   三者都能进同一个队列，且各自类型安全。
        //
        // ⚠️ 为什么需要延迟：
        //   回收动作往往在【对象自己的回调里】产生（如 WindowDestroy 回调里
        //   要删掉发事件的窗口）。而此刻 dispatcher 正在遍历绑定表、
        //   ProcessEvents 还在栈上，全都攥着这个对象的指针 ——
        //   当场 delete 会立刻 迭代器失效 + UAF。
        //   ⇒ 只登记动作，等这一轮事件派发彻底结束、没有任何栈帧引用它们了，
        //     再在安全点统一执行。
        //
        // ⚠️ 清理责任：队列【由 FlushDeferredRecycle 自己清空】，
        //   每轮 ProcessEvents 末尾必定 flush 掉，不存在"残留已完成项"。
        //   所以它不是一个"待监控对象清单"，无需"对象死了要摘除"的逻辑 ——
        //   动作执行的瞬间就是对象真正死亡的瞬间（动作即死亡证明）。
        //
        // ⚠️ 防重复入队：同一对象入队两次 = flush 时 delete 两遍 = 崩溃。
        //   窗口侧在 XWidget 里用 m_RecycleQueued 门闩自保（见 XWidget.h）。
        // ============================================================
        std::vector<std::function<void()>> m_DeferredRecycle;

    public:
        // 1.构造、析构放protected，允许子类继承构造，禁止外部new
        Application(int argc, char *argv[]);

        virtual ~Application(); // 2.虚析构，多态析构必备

        // ── 延迟回收：把一个回收动作推迟到本轮事件处理结束后的安全点执行 ──
        // ⚠️ 只对 new 出来的对象用（动作里通常是 delete）。
        // ⚠️ 别在回调里直接 delete 自己 —— 用这个入队，别自己动手。
        void DeferRecycle(std::function<void()> action)
        {
            if (action)
                m_DeferredRecycle.push_back(std::move(action));
        }

        // 执行并清空队列（在 ProcessEvents 一轮末尾调）。
        // 内部先 swap 取走再逐个执行：动作本身可能又入队新动作，
        // 这样不会"自己吃自己"（遍历中往同一容器 push 会迭代器失效）。
        void FlushDeferredRecycle();

        // 消息循环
        virtual void exec();
        virtual void pushEvents();
        virtual void pushEvents(XMovement *e);
        virtual void ProcessEvents();
        bool isRunning() { return Running; }
        void appClose() { Running = false; }
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
        // ── 主窗口（退出语义）────────────────────────────────
        // 显式指定"关掉它就退出程序"的窗口。
        //   w == nullptr → 取消主窗口（此后没有任何窗口负责退出）。
        // 内部把 w 的 WindowClose 接到 appClose；并在 w 被原生销毁时清掉引用。
        void SetMainWindow(XWidget *w);
        XWidget *GetMainWindow() const { return m_MainWindow; }

        // ── 托管 / 退出清理 ────────────────────────────
        // 把一个【new 出来的顶层窗口】交给 Application 托管：程序退出时若它还
        // 活着，会被 Shutdown 自动 destroy + 回收。重复托管同一窗口幂等。
        void Own(XWidget *w);
        // 窗口销毁时由 XWidget 回调：从名单摘除（不拥有、不 delete）。
        void ForgetOwnedWindow(XWidget *w);
        // 显式退出清理：销毁所有还活着的托管窗口，并泵消息把
        // 「destroy → WM_DESTROY → WindowDestroy → 延迟回收」走完，
        // 保证析构/线程停止/资源释放都跑到。
        // ⚠️ 请在【主循环结束后】（如 main 末尾）调用，别在回调里调。
        void Shutdown();
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
