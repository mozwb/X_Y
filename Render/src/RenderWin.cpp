#include"renderWin/renderWin.h"
namespace X_Y {
	RenderWin::RenderWin(XWidget* parent): XWidget(parent){
        connect(
            this,
            RenderType::RenderStart,
            Render::instance(),
            &Render::submitEvent
        );
	}
    void RenderWin::RenderInit() {

        RenderStart* e = new RenderStart(this, 
            this->get_context(),this->get_width(),this->get_height() 
        );
        if (e&&this->get_context()) {
            XDEBUG("RenderStart发送")
            this->emit(e);
        }
    }
    void RenderWin::emit(RenderEvent* e) {
        auto app = Application::instance();
        app->pushEvents(e);
    }
}