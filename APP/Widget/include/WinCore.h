#pragma once
#include"Log/include/XYLog.h"
#include"Application/include/Application.h"
#include"Movement/include/movements.h"
#include"Movement/include/AppMovement.h"
#include"Movement/include/KeyMovement.h"
#include"Movement/include/MouseMovement.h"
#include"Input/include/MapCode.h"
#include"BaseWin.h"
#ifdef XY_PLATFORM_WINDOWS
#include<windows.h>
#include <tchar.h>

namespace X_Y::WinCore {
    using BaseWin = X_Y::BaseWin;
        inline   WNDCLASSEX g_XYWindowClass = { 0 };
        inline  HINSTANCE g_hInstance = nullptr;
        inline  const TCHAR* g_szClassName = _T("X_YWindow");
        inline void SetHinstace(HINSTANCE& hInstance) { g_hInstance = hInstance;   }

    // 可选 WndProc hook：UI 模块可在初始化时注册
    // 返回 true 表示消息已被处理，Window 不再转发
    using WndProcHookFn = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);
    inline WndProcHookFn g_WndProcHook = nullptr;
    inline WndProcHookFn g_ContainerHook = nullptr;

    inline LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // 2. UI hook（绘制相关）
        if (g_ContainerHook) {
            LRESULT uiResult = g_ContainerHook(hwnd, msg, wParam, lParam);
            if (uiResult == 0) // 返回 0 表示已处理
                return 0;
        }
      // 3. 可选 hook（如 ImGui 输入优先处理）
        if (g_WndProcHook && g_WndProcHook(hwnd, msg, wParam, lParam))
            return true;// g_WndProcHook(hwnd, msg, wParam, lParam)返回的基本是0,所以这里基本不会触发，但是不影响
        BaseWin* pThis = nullptr;
            auto* app = Application::instance();
            // 1. 窗口创建时：绑定 C++ 对象与 HWND，记录初始尺寸
            if (msg == WM_NCCREATE) {
                pThis = (BaseWin*)((CREATESTRUCT*)lParam)->lpCreateParams;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
                pThis->SetNHWD(hwnd);

                RECT rect;
                if (GetClientRect(hwnd, &rect))
                    pThis->SetActualSize(rect.right, rect.bottom);
            }
            else {
                // 其他消息：从窗口附加数据取出对象
                pThis = (BaseWin*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            }
            Movement* movement=nullptr;
            KeyCode key = NULL;
            MouseCode mbutoon = NULL;


            // 2. 如果对象存在，把 Win32 消息转换成 MovementType 事件并转发
            if (pThis)
            {
                switch (msg)
                {
                    // 窗口关闭
                case WM_CLOSE:
                {
                    movement = new WindowClose(pThis);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }
                    // 窗口销毁
                case WM_DESTROY:
                {
                    movement = new WindowDestory(pThis);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }
                    // 窗口大小改变
                case WM_SIZE:
                {
                    int width = LOWORD(lParam);
                    int height = HIWORD(lParam);
                    // 先更新实际尺寸，再派发事件（事件只用于通知）
                    if (pThis)
                        pThis->SetActualSize(width, height);
                    movement = new WindowResize(pThis,width,height);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 窗口移动
                case WM_MOVE:
                {
                     movement = new WindowMoved(pThis);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }
                //case WM_PAINT:
                //{
                //    movement = new WindowPaint(pThis);
                //    app->GetEventQueue().Push(movement);
                //    return 0;
                //}
                // 窗口焦点变化
                case WM_SETFOCUS:
                {
                    movement = new WindowFouces(pThis);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 标题栏拖动开始
                case WM_NCLBUTTONDOWN:
                {
                    if (wParam == HTCAPTION) {
                        movement = new WindowDragBegin(pThis);
                        app->GetEventQueue().Push(movement);
                    }
                    break;  // 不拦截，让 DefWindowProc 处理默认拖动
                }
                // 拖动 / 大小调整结束
                case WM_EXITSIZEMOVE:
                {
                    movement = new WindowDragEnd(pThis);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                    // 键盘按下
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                {
                    bool isRepeat = (lParam & (1 << 30)) != 0;
                    key = InputMapping::Translate(wParam);
                    movement = new KeyPressed(pThis, key, isRepeat);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }
                    // 键盘抬起
                case WM_KEYUP:
                case WM_SYSKEYUP:
                {
                    key = InputMapping::Translate(wParam);
                    movement = new KeyReleased(pThis, key);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }
                    // 鼠标移动
                case WM_MOUSEMOVE:
                {
                    float x = (short)LOWORD(lParam);
                    float y = (short)HIWORD(lParam);
                     movement = new MouseMoved(pThis,x,y);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 鼠标左键按下
                case WM_LBUTTONDOWN:
                {
                    mbutoon = InputMapping::TranslateMouse(VK_LBUTTON);
                     movement = new MouseButtonPressed(pThis,mbutoon);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 鼠标左键抬起
                case WM_LBUTTONUP:
                {
                     mbutoon = InputMapping::TranslateMouse(VK_LBUTTON);
                     movement = new MouseButtonReleased(pThis, mbutoon);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 鼠标右键按下
                case WM_RBUTTONDOWN:
                {
                    mbutoon = InputMapping::TranslateMouse(VK_RBUTTON);
                     movement = new MouseButtonPressed(pThis, mbutoon);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 鼠标右键抬起
                case WM_RBUTTONUP:
                {
                     mbutoon = InputMapping::TranslateMouse(VK_RBUTTON);
                     movement = new MouseButtonReleased(pThis, mbutoon);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }

                // 鼠标滚轮(垂直滚轮)
                case WM_MOUSEWHEEL:
                {
                    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    float yOffset = static_cast<float>(delta) / WHEEL_DELTA;
                    movement = new MouseScrolled(pThis,0.0,yOffset);
                    app->GetEventQueue().Push(movement);
                    return 0;
                }
                }
            }

     

            // 4. 未处理的消息 → 交给系统默认处理
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
            XINFO("hInstance:{}",hInstance)
            InitGlobalWindowClass(hInstance);
            if (!RegisterClassEx(&g_XYWindowClass))
            {
                MessageBox(NULL, _T("窗口类注册失败"), _T("错误"), MB_ICONERROR);
                exit(EXIT_FAILURE);
            }
        }
    }

#endif // 
