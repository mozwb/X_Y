#include "Container/Container.h"
#include "Component/Component.h"
#include "Component/ScrollArea.h"
#include "Movement/include/MouseMovement.h"
#include "Movement/include/KeyMovement.h"

namespace X_Y {

#ifdef XY_PLATFORM_WINDOWS
    namespace {
        bool g_HookRegistered = false;
        struct HookCleanup {
            ~HookCleanup() {
                WinCore::g_ContainerHook = nullptr;
            }
        };
        HookCleanup g_Cleanup;
    }
#endif

    Container::Container(XWidget* parent)
        : XWidget(parent)
    {
#ifdef XY_PLATFORM_WINDOWS
        if (!g_HookRegistered) {
            WinCore::g_ContainerHook = ContainerWndProc;
            g_HookRegistered = true;
        }
#endif
    }

    Container::~Container() {
        ClearComponents();
    }

    void Container::AddComponent(Component* comp) {
        if (comp) {
            m_Components.push_back(comp);
        }
    }

    void Container::RemoveComponent(Component* comp) {
        auto it = std::find(m_Components.begin(), m_Components.end(), comp);
        if (it != m_Components.end()) {
            m_Components.erase(it);
        }
    }

    void Container::ClearComponents() {
        m_Components.clear();
    }

    Component* Container::HitTest(int x, int y) {
        for (auto it = m_Components.rbegin(); it != m_Components.rend(); ++it) {
            Component* comp = *it;
            int cx = comp->GetX();
            int cy = comp->GetY();
            int cw = comp->GetWidth();
            int ch = comp->GetHeight();
            if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
                return comp;
            }
        }
        return nullptr;
    }

    void Container::OnPaint(Canvas& canvas) {
        for (auto* comp : m_Components) {
            if (comp->IsVisible()) {
                canvas.SetClip(
                    comp->GetX(), comp->GetY(),
                    comp->GetWidth(), comp->GetHeight()
                );
                comp->OnPaint(canvas);
                canvas.ResetClip();
            }
        }
    }

#ifdef XY_PLATFORM_WINDOWS

    LRESULT CALLBACK Container::ContainerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        BaseWin* pThis = (BaseWin*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!pThis) return 1;

        Container* container = dynamic_cast<Container*>(pThis);
        if (!container) return 1;

        auto* app = Application::instance();

        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT rect;
                GetClientRect(hwnd, &rect);
                int w = rect.right - rect.left;
                int h = rect.bottom - rect.top;

                Canvas canvas(w, h, hdc);
                container->OnPaint(canvas);

                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_LBUTTONDOWN: {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                Component* hit = container->HitTest(x, y);
                if (hit) {
                    hit->SetMouseLocal(x - hit->GetX(), y - hit->GetY());
                    hit->SetFocused(true);

                    for (auto* c : container->m_Components) {
                        if (c != hit) c->SetFocused(false);
                    }

                    MouseCode btn = InputMapping::TranslateMouse(VK_LBUTTON);
                    auto* e = new MouseButtonPressed(hit, btn);
                    app->GetEventQueue().Push(e);

                    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
                }
                return 0;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                bool isRepeat = (lParam & (1 << 30)) != 0;
                KeyCode key = InputMapping::Translate((uint32_t)wParam);

                // Keyboard: push 到事件队列，sender = Container
                auto* e = new KeyPressed(container, key, isRepeat);
                app->GetEventQueue().Push(e);

                // 同时告诉焦点的 Component（用于输入框文字处理）
                for (auto* comp : container->m_Components) {
                    if (comp->IsVisible() && comp->IsFocused()) {
                        comp->OnKeyDown((int)wParam);
                        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
                        break;
                    }
                }
                return 0;
            }

            case WM_CHAR: {
                // 推送到事件队列
                auto* e = new KeyTyped(container, (uint32_t)wParam);
                app->GetEventQueue().Push(e);

                // 同时告诉焦点的 Component
                for (auto* comp : container->m_Components) {
                    if (comp->IsVisible() && comp->IsFocused()) {
                        comp->OnChar((wchar_t)wParam);
                        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
                        break;
                    }
                }
                return 0;
            }

            case WM_MOUSEWHEEL: {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                for (auto* comp : container->m_Components) {
                    if (!comp->IsVisible()) continue;
                    int cx = comp->GetX(), cy = comp->GetY();
                    int cw = comp->GetWidth(), ch = comp->GetHeight();
                    if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
                        auto* sa = dynamic_cast<ScrollArea*>(comp);
                        if (sa) {
                            sa->ScrollBy(delta / 4);
                            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
                        }
                        break;
                    }
                }
                return 0;
            }
        }
        return 1;
    }

#endif // XY_PLATFORM_WINDOWS

}
