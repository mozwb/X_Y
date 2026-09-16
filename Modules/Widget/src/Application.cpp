#include "Application.h"
#include "Widget/BaseWin.h"
#include "Widget/PlatformLoop.h"

namespace X_Y
{

    Application::Application(int argc, char *argv[])
    {
        if (!s_instance)
        {
            s_instance = this;
            m_PlatformLoop.reset(PlatformLoopFactory::Create());
            m_PlatformLoop->Boot();
        }
    }

    Application::~Application()
    {
        // 兜底：退出时可能还有没来得及回收的对象（比如窗口已 Destroy 但
        // 事件循环没再转一轮）。这里最后清一次，避免进程退出前泄漏。
        FlushDeferredRecycle();
        s_instance = nullptr;
    }

    // ── 延迟回收 ──────────────────────────────────
    // ⚠️ 先 swap 取走再执行：
    //   1. 动作里可能又 DeferRecycle（如析构期间又关了别的窗口）—— 若直接遍历
    //      原 vector 且边执行边 push_back，迭代器失效。
    //   2. 执行期间 m_DeferredRecycle 保持"可接收新入队"的正常状态。
    void Application::FlushDeferredRecycle()
    {
        if (m_DeferredRecycle.empty())
            return;

        std::vector<std::function<void()>> actions;
        actions.swap(m_DeferredRecycle); // 队列立刻变空，本轮已取走

        XDEBUG("[回收安全点] 本轮待回收动作 {} 个", actions.size())

        for (auto &action : actions)
        {
            if (action)
                action(); // 执行（通常是 delete this）
        }
        // actions 析构 → 动作对象本身也没了。队列无残留，无需"摘除"逻辑。
    }

    void Application::exec()
    {
        while (Running)
        {
            pushEvents();
            ProcessEvents();
        }
    }

    void Application::pushEvents()
    {
        if (m_PlatformLoop)
        {
            m_PlatformLoop->PumpMessage();
        }
    }

    void Application::pushEvents(XMovement *e)
    {
        m_eventQueue.Push(e);
    }

    void Application::ProcessEvents()
    {
        while (!m_eventQueue.Empty())
        {
            X_Y::XMovement *baseEvt = m_eventQueue.Pop();
            if (!baseEvt)
                continue;

            for (auto *layer : m_LayerStack.GetLayers())
            {
                layer->OnEvent(baseEvt);
                if (baseEvt->Handled)
                    break;
            }

            // 全局鼠标钩子：命中重定向（方案A预埋，本次默认空）。
            // 在 dispatcher 之前执行，保证"布局层优先于子窗口"（如分割线优先于 Dock 内容点击）。
            // 返回 true 表示钩子已吞掉该事件，不再进 dispatcher。
            if (!baseEvt->Handled && m_MouseRedirectHandler)
            {
                if (m_MouseRedirectHandler(*baseEvt))
                    baseEvt->Handled = true;
            }

            if (!baseEvt->Handled)
            {
                baseEvt->DispatchEvent(static_cast<void *>(&m_dispatcher));
            }

            delete baseEvt;
        }

        // ════════════════════════════════════════════════════════
        // 安全点：本轮事件全部派发完毕，所有回调栈帧都已返回。
        // 此刻才执行延迟回收 —— 这些被回收的对象（如刚销毁的窗口）在
        // 派发期间还被 dispatcher / ProcessEvents 引用着，当场 delete 就是 UAF。
        // ⚠️ 必须在 while 之外：在循环里 flush 的话，队列里其它事件可能
        //    还引用着已被回收的 sender。
        // ════════════════════════════════════════════════════════
        FlushDeferredRecycle();
    }

}
