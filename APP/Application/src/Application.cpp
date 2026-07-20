#include "Application.h"
#include "Widget/include/BaseWin.h"
#include "Widget/include/PlatformLoop.h"

namespace X_Y {

Application::Application(int argc, char* argv[])
{
    s_instance = this;
    m_PlatformLoop.reset(PlatformLoopFactory::Create());
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

void Application::pushEvents() {
    if (m_PlatformLoop) {
        m_PlatformLoop->PumpMessage();
    }
}

void Application::pushEvents(XMovement* e) {
    m_eventQueue.Push(e);
}

void Application::ProcessEvents() {
    while (!m_eventQueue.Empty()) {
        X_Y::XMovement* baseEvt = m_eventQueue.Pop();
        if (!baseEvt) continue;

        for (auto* layer : m_LayerStack.GetLayers())
        {
            layer->OnEvent(baseEvt);
            if (baseEvt->Handled)
                break;
        }

        if (!baseEvt->Handled)
        {
            baseEvt->DispatchEvent(static_cast<void*>(&m_dispatcher));
        }

        delete baseEvt;
    }
}

}
