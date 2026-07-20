#include "Win32/Win32WndProc.h"
#include "Win32/Win32Globals.h"
#include "BaseWin.h"
#include "Application/include/Application.h"
#include "Movement/include/movements.h"
#include "Movement/include/AppMovement.h"
#include "Movement/include/KeyMovement.h"
#include "Movement/include/MouseMovement.h"
#include "Canvas.h"
#include "Input/include/MapCode.h"

namespace X_Y::Win32 {

LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    using BaseWin = X_Y::BaseWin;

    // 1. WndProc hook（如 ImGui 输入优先处理）
    if (g_WndProcHook && g_WndProcHook(hwnd, msg, wParam, lParam))
        return true;

    BaseWin* pThis = nullptr;
    auto* app = Application::instance();

    // 2. 窗口创建时：绑定 C++ 对象与 HWND，记录初始尺寸
    if (msg == WM_NCCREATE) {
        pThis = (BaseWin*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->SetNativeHandle((void*)hwnd);

        RECT rect;
        if (GetClientRect(hwnd, &rect))
            pThis->SetActualSize(rect.right, rect.bottom);

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    else {
        pThis = (BaseWin*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    // 3. WM_PAINT：创建 Canvas 后通过虚函数 OnPaint 回调
    if (msg == WM_PAINT && pThis) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        Canvas canvas(w, h, (void*)hdc);
        pThis->OnPaint(&canvas);

        EndPaint(hwnd, &ps);
        return 0;
    }

    Movement* movement = nullptr;
    KeyCode key = NULL;
    MouseCode mbutton = NULL;

    // 4. 如果对象存在，把 Win32 消息转换成 MovementType 事件
    if (pThis) {
        switch (msg) {
        case WM_CLOSE: {
            movement = new WindowClose(pThis);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_DESTROY: {
            movement = new WindowDestroy(pThis);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            pThis->SetActualSize(width, height);
            movement = new WindowResize(pThis, width, height);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_MOVE: {
            movement = new WindowMoved(pThis);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_SETFOCUS: {
            movement = new WindowFouces(pThis);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_NCLBUTTONDOWN: {
            if (wParam == HTCAPTION) {
                movement = new WindowDragBegin(pThis);
                app->GetEventQueue().Push(movement);
            }
            break;
        }
        case WM_EXITSIZEMOVE: {
            movement = new WindowDragEnd(pThis);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            bool isRepeat = (lParam & (1 << 30)) != 0;
            key = InputMapping::Translate(wParam);
            movement = new KeyPressed(pThis, key, isRepeat);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            key = InputMapping::Translate(wParam);
            movement = new KeyReleased(pThis, key);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_MOUSEMOVE: {
            float x = (short)LOWORD(lParam);
            float y = (short)HIWORD(lParam);
            movement = new MouseMoved(pThis, x, y);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            mbutton = InputMapping::TranslateMouse(VK_LBUTTON);
            movement = new MouseButtonPressed(pThis, mbutton);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_LBUTTONUP: {
            mbutton = InputMapping::TranslateMouse(VK_LBUTTON);
            movement = new MouseButtonReleased(pThis, mbutton);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            mbutton = InputMapping::TranslateMouse(VK_RBUTTON);
            movement = new MouseButtonPressed(pThis, mbutton);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_RBUTTONUP: {
            mbutton = InputMapping::TranslateMouse(VK_RBUTTON);
            movement = new MouseButtonReleased(pThis, mbutton);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float yOffset = static_cast<float>(delta) / WHEEL_DELTA;
            movement = new MouseScrolled(pThis, 0.0, yOffset);
            app->GetEventQueue().Push(movement);
            return 0;
        }
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} 
// namespace X_Y::Win32
