#include"Window/src/windows/XWidget.h"
#include"Log/src/XYLog.h"
namespace X_Y {
#ifdef XY_PLATFORM_WINDOWS
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
        }
        bool XWidget::show(showtype nShow)
        {
            // 1. 已经有窗口句柄 → 直接显示
            HWND hwnd = GetHwnd();
            if (hwnd) {
                ShowWindow(hwnd, nShow);
                UpdateWindow(hwnd);
                return true;
            }

            // 2. 没有句柄 → 只创建一次！！！
            XINFO("窗口未创建，自动创建窗口: {}", m_title);
            if (!create(m_title, m_width, m_height)) {
                XERROR("自动创建窗口失败！");
                return false;
            }

            // 3. 创建成功后再显示
            hwnd = GetHwnd();
            if (hwnd) {
                ShowWindow(hwnd, nShow);
                UpdateWindow(hwnd);
            }

            return true;
        }
        void XWidget::setTitle(const char* title) {
            m_title = title;
            HWND hwnd = GetHwnd();
            if (hwnd)  // 你窗口句柄，一般叫 m_hwnd / m_handle / m_window
            {
                SetWindowTextA(hwnd, title);
            }
        }

        void XWidget::setSize(uint width, uint height) {
            m_width = width;
            m_height = height;
        }
        void XWidget::close() {
            XINFO("调用colse函数")
            HWND hwnd = this->GetHwnd();
            if (hwnd) {
                this->show(HIDE);
            }
        }
        void XWidget::destroy() {
            HWND hwnd = this->GetHwnd();
            disConnect(this);
            if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
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
        bool XWidget::create(const char* title, uint width, uint height){
            HINSTANCE hInstance = X_Y::WinCore::g_hInstance;

            // 1. 把 char* 变量安全转为宽字符串
            wchar_t wTitle[256] = { 0 };

            // 🔥 这里必须用 CP_ACP（Windows 本地编码）
            MultiByteToWideChar(CP_ACP, 0, title, -1, wTitle, _countof(wTitle));

            HWND hwnd = CreateWindowEx(
                0,
                X_Y::WinCore::g_szClassName,         
                wTitle,
                WS_OVERLAPPEDWINDOW,   
                CW_USEDEFAULT, CW_USEDEFAULT,
                width, height,
                NULL, NULL,
                hInstance,            
                this                 
            );
                if (!hwnd) {
                DWORD err = GetLastError();
                XERROR("CreateWindowEx failed, error code: {}", err);
                return false;
                }
            XINFO("hwnd:{}", (uint64_t)hwnd)
            if (hwnd) {
                SetHwnd(hwnd);
                return true;
            }
            return false;
        }
#endif
 }