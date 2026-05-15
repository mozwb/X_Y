#pragma once
#include<windows.h>
#include <tchar.h>
#include"Log/src/XYLog.h"
namespace X_Y {
    namespace WinCore {
        inline   WNDCLASSEX g_XYWindowClass = { 0 };
        inline  HINSTANCE g_hInstance = nullptr;
        inline  const TCHAR* g_szClassName = _T("X_YWindow");
        inline void SetHinstace(HINSTANCE& hInstance) { g_hInstance = hInstance;   }
        //只负责管理HWND和消息分发，不负责任何实现
        class BaseWin
        {
        public:
            BaseWin() = default;
            void SetHwnd(HWND& hwnd){ m_hwnd = hwnd; }
            HWND GetHwnd()const { return m_hwnd; }
            virtual void OnCreate() {}
            virtual void OnClose() {}
            virtual void OnDestroy() {}
            virtual void OnPaint() {}
            virtual void OnSize(int width, int height) {}
            virtual void OnMouseMove(int x, int y) {}
            virtual void OnLButtonDown(int x, int y) {}
            virtual void OnLButtonUp(int x, int y) {}
            virtual void OnRButtonDown(int x, int y) {}
            virtual void OnKeyDown(int key) {}
            virtual void OnKeyUp(int key) {}
        private:
            HWND m_hwnd = nullptr; 
        };


       inline  LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            BaseWin* pThis = nullptr;

            if (msg == WM_NCCREATE) {
                pThis = (BaseWin*)((CREATESTRUCT*)lParam)->lpCreateParams;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
                pThis->SetHwnd(hwnd);
            }
            else {
                pThis = (BaseWin*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            }

            if (pThis)
            {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);

                switch (msg)
                {
                case WM_CREATE:        pThis->OnCreate();               break;
                case WM_CLOSE:         pThis->OnClose();                break;
                case WM_DESTROY:       pThis->OnDestroy();              break;
                case WM_PAINT:         pThis->OnPaint();                break;
                case WM_SIZE:          pThis->OnSize(w, h);             break;
                case WM_MOUSEMOVE:     pThis->OnMouseMove(x, y);        break;
                case WM_LBUTTONDOWN:   pThis->OnLButtonDown(x, y);      break;
                case WM_LBUTTONUP:     pThis->OnLButtonUp(x, y);        break;
                case WM_RBUTTONDOWN:   pThis->OnRButtonDown(x, y);      break;
                case WM_KEYDOWN:       pThis->OnKeyDown((int)wParam);   break;
                case WM_KEYUP:         pThis->OnKeyUp((int)wParam);     break;
                }
            }

            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
       inline void InitGlobalWindowClass(HINSTANCE& hInstance)
        {
            // 已经初始化过就不再初始化
            if (g_XYWindowClass.cbSize != 0)
                return;

            // 你写的完美配置
            g_XYWindowClass.cbSize = sizeof(WNDCLASSEX);
            g_XYWindowClass.hInstance = hInstance;
            g_XYWindowClass.lpszClassName = g_szClassName;
            g_XYWindowClass.lpfnWndProc = StaticWndProc;
            g_XYWindowClass.style = CS_HREDRAW | CS_VREDRAW;
            g_XYWindowClass.cbClsExtra = 0;
            g_XYWindowClass.cbWndExtra = 0;
            g_XYWindowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
            g_XYWindowClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
            g_XYWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
            g_XYWindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            g_XYWindowClass.lpszMenuName = NULL;
        }
        inline void RegisterWinClass(HINSTANCE& hInstance) {
            INFO("hInstance:{}",hInstance)
            InitGlobalWindowClass(hInstance);
            if (!RegisterClassEx(&g_XYWindowClass))
            {
                MessageBox(NULL, _T("窗口类注册失败"), _T("错误"), MB_ICONERROR);
                exit(EXIT_FAILURE);
            }
        }
    }
}
