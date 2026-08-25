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
        s_instance = nullptr;
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
    }

}
