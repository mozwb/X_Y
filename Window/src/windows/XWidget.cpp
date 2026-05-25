#include"Window/src/windows/XWidget.h"
#include"Log/src/XYLog.h"
namespace X_Y {
        XWidget::XWidget(const char* title, uint width, uint height):m_title(title) 
         ,m_width(width),m_height(height)
        {
        
            connect(this, MovementType::WindowClose, this, &XWidget::destroy);
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
        void XWidget::close() {
            XINFO("调用colse函数")
            HWND hwnd = this->GetHwnd();
            if (hwnd) {
                this->show(HIDE);
            }
        }
        void XWidget::destroy() {
            HWND hwnd = this->GetHwnd();
            DisConnect(this);
            if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
        }

#ifdef XY_PLATFORM_WINDOWS
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