#include"XWidget.h"
#include"Log/include/XYLog.h"
#include"Application/include/Application.h"
#include"WinCore.h"
namespace X_Y {
        XWidget::XWidget(XWidget* parent):
            m_parent(parent)
        {
            XDEBUG("XWidget 构造执行，parent={}", (uint16_t)parent)
            auto app = Application::instance();
            if (!app) {
                // 处理单例为空的异常，比如直接assert或日志
                XY_CORE_ASSERT(false,"Application instance is null!");
                return;
            }

            if (m_parent) {
                // 子窗口：绑定自身销毁和父窗口关闭时销毁
                XDEBUG("子窗口构造执行")
                connect(this, MovementType::WindowClose, this, &XWidget::destroy);
                connect(parent, MovementType::WindowClose, this, &XWidget::destroy);
            }
            else if (!app->IsFirstWin()) {
                // 顶级窗口：更新主窗口标记，并绑定到应用退出
                app->updateFirstWin();
                connect(this, MovementType::WindowClose, app, &Application::appClose);
            }
            else {
                //主窗口逻辑
                connect(this, MovementType::WindowClose, this, &XWidget::destroy);
            }

            // 窗口 resize 时打印日志
            Connect(this, MovementType::WindowResize, this, [this](const XMovement& e) {
                auto& resize = dynamic_cast<const WindowResize&>(e);
                XDEBUG("窗口resize: {}x{}", resize.GetWidth(), resize.GetHeight());
            });
        }
        bool XWidget::show(showtype nShow)
        {   

            if (this->Show(nShow)) return true;
            // 2. 没有句柄 → 只创建一次！！！
            XINFO("窗口未创建，自动创建窗口: {}", m_title);
            if (this->Create(m_title, GetActualWidth(), GetActualHeight())) {
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
        void XWidget::close() {
            XINFO("调用colse函数")
                this->Close();
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
            m_parent=nullptr;
        }

 }