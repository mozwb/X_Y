#include"renderWin/renderWin.h"
namespace X_Y {
	RenderWin::RenderWin(XWidget* parent): XWidget(parent){}
    void RenderWin::RenderInit() {

        RenderStart* e = new RenderStart(this, 
            this->get_context(),this->get_width(),this->get_height() 
        );
        if (e&&this->get_context()) {
            XDEBUG("RenderStart发送")
            auto dispatcher = X_Y::MovementDispatcher();
			dispatcher.Connect(
				this,
				RenderType::RenderStart,
				Render::instance(),
				[this](const XMovement& e) {
					auto& renderStart = dynamic_cast<const RenderStart&>(e);
					XDEBUG("RenderStart接收")
						Render::instance()->submitEvent(const_cast<RenderStart*>(&renderStart));
				}
			);
            e->DispatchEvent(static_cast<void*>(&dispatcher));
            delete e;
        }
    }
    void RenderWin::emit(RenderEvent* e) {
        auto app = Application::instance();
        app->pushEvents(e);
    }
}