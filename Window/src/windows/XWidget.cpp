#include"Window/src/windows/XWidget.h"
#include"Log/src/XYLog.h"
namespace X_Y {
        XWidget::XWidget(const char* title, uint width, uint height):m_title(title) 
         ,m_width(width),m_height(height)
        {}
        void XWidget::show(int nShow)
        {
            if (!create(m_title, m_width,m_height)) {
                ERROR("{}:无法创建窗口",this)
                //std::cerr << "无法创建窗口" << std::endl;

            }
            HWND hwnd = this->GetHwnd();
            if (hwnd)
            {
                ShowWindow(hwnd, nShow);
                UpdateWindow(hwnd);
            }
        }
        void XWidget::close() {
            HWND hwnd = this->GetHwnd();
            if (hwnd)   SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
        void XWidget::destroy() {
            HWND hwnd = this->GetHwnd();
            if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
        }

        bool XWidget::create(const char* title, uint width, uint height){
            HINSTANCE hInstance = X_Y::WinCore::g_hInstance;
            HWND hwnd = CreateWindowEx(
                0,
                X_Y::WinCore::g_szClassName,         
                LPCWSTR(title),                
                WS_OVERLAPPEDWINDOW,   
                CW_USEDEFAULT, CW_USEDEFAULT,
                width, height,
                NULL, NULL,
                hInstance,            
                this                 
            );
                if (!hwnd) {
                DWORD err = GetLastError();
                ERROR("CreateWindowEx failed, error code: {}", err);
                return false;
                }
            INFO("hwnd:{}",hwnd)
            if (hwnd) {
                SetHwnd(hwnd);
                return true;
            }
            return false;
        }
 }
