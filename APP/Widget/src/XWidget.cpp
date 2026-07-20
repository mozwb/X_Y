#include "XWidget.h"
#include "Log/include/XYLog.h"


namespace X_Y {

XWidget::XWidget(XWidget* parent)
    : m_parent(parent)
{
    XDEBUG("XWidget 构造执行，parent={}", (uint16_t)parent)
    auto app = Application::instance();
    if (!app) {
        XY_CORE_ASSERT(false, "Application instance is null!");
        return;
    }

    if (m_parent) {
        XDEBUG("子窗口构造执行")
        connect(this, MovementType::WindowClose, this, &XWidget::destroy);
        connect(parent, MovementType::WindowClose, this, &XWidget::destroy);
    }
    else if (!app->IsFirstWin()) {
        app->updateFirstWin();
        connect(this, MovementType::WindowClose, app, &Application::appClose);
    }
    else {
        connect(this, MovementType::WindowClose, this, &XWidget::destroy);
    }

    Connect(this, MovementType::WindowResize, this, [this](const XMovement& e) {
        auto& resize = dynamic_cast<const WindowResize&>(e);
        XDEBUG("窗口resize: {}x{}", resize.GetWidth(), resize.GetHeight());
    });
}

bool XWidget::show(ShowCmd nShow) {
    if (BaseWin::Show(nShow)) return true;
    XINFO("窗口未创建，自动创建窗口: {}", m_title);
    if (this->Create(m_title, GetActualWidth(), GetActualHeight(),
                     m_WindowStyle, m_ParentHwnd)) {
        return this->show(nShow);
    }
    XERROR("窗口创建失败")
    return false;
}

void XWidget::setTitle(const char* title) {
    m_title = title;
    this->SetTitle(title);
}

void XWidget::setSize(uint width, uint height) {
    SetActualSize(width, height);
}

void XWidget::destroy() {
    disConnect(this);
    this->Destroy();
}

void XWidget::disconnectPa() {
    if (m_parent) {
        disConnect(m_parent, this);
    }
}

void XWidget::releaseSelf() {
    disconnectPa();
    m_parent = nullptr;
}

}
