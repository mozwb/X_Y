#include "Input.h"
#include "KeyMapper.h"

#ifdef XY_PLATFORM_WINDOWS
// OEMRESOURCE 让 winuser.h 暴露 OCR_* 系统光标 id（SetSystemCursor 用）
#define OEMRESOURCE
#include <windows.h>
#endif

namespace X_Y
{

    // 全局 mapper 实例
    static KeyMapper *s_Mapper = nullptr;

    static KeyMapper *GetMapper()
    {
        if (!s_Mapper)
        {
            s_Mapper = KeyMapperFactory::Create();
        }
        return s_Mapper;
    }

    namespace Input_t
    {

        bool Input::IsKeyPressed(KeyCode key)
        {
            auto *mapper = GetMapper();
            return mapper->IsKeyPressed(static_cast<uint32_t>(key));
        }

        bool Input::IsMouseButtonPressed(MouseCode button)
        {
            auto *mapper = GetMapper();
            return mapper->IsMousePressed(static_cast<uint32_t>(button));
        }

        xpos Input::GetMousePosition()
        {
            float x, y;
            GetMapper()->GetMousePos(x, y);
            return {x, y};
        }

        float Input::GetMouseX()
        {
            return GetMousePosition().x;
        }

        float Input::GetMouseY()
        {
            return GetMousePosition().y;
        }

        void Input::SetMousePosition(float x, float y)
        {
            GetMapper()->SetMousePos(x, y);
        }

        // ── 设备层(全局)能力 ──────────────────────

        void Input::SetCursorVisible(bool visible)
        {
#ifdef XY_PLATFORM_WINDOWS
            if (visible)
            {
                // 恢复系统默认光标
                ::SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
            }
            else
            {
                // 全局彻底隐藏：用 1x1 全透明光标替换系统光标（SetSystemCursor）。
                // 注意：ShowCursor 只对“本线程激活窗口”生效，无焦点/全屏透明层无效，
                //       故用 SetSystemCursor 全局替换，任何窗口都看不到光标。
                static HCURSOR s_hidden = nullptr;
                if (!s_hidden)
                {
                    // 1x1 透明位图掩码：AND 全 1 + XOR 全 0 = 全透明
                    static const BYTE andMask[1] = {0xFF};
                    static const BYTE xorMask[1] = {0x00};
                    s_hidden = ::CreateCursor(::GetModuleHandleW(nullptr),
                                              0, 0, 1, 1, andMask, xorMask);
                }
                if (s_hidden)
                {
                    // 替换主要系统光标（SetSystemCursor 会销毁传入句柄，故 CopyIcon）
                    static const int ids[] = {
                        OCR_NORMAL,
                        OCR_IBEAM,
                        OCR_WAIT,
                        OCR_CROSS,
                        OCR_SIZEALL,
                        OCR_SIZENWSE,
                        OCR_SIZENESW,
                        OCR_SIZEWE,
                        OCR_SIZENS,
                        OCR_NO,
                        OCR_HAND,
                        OCR_APPSTARTING,
                    };
                    for (int id : ids)
                        ::SetSystemCursor(::CopyIcon(s_hidden), id);
                }
            }
#endif
        }

        int Input::GetScreenWidth()
        {
#ifdef XY_PLATFORM_WINDOWS
            return ::GetSystemMetrics(SM_CXSCREEN);
#else
            return 0;
#endif
        }

        int Input::GetScreenHeight()
        {
#ifdef XY_PLATFORM_WINDOWS
            return ::GetSystemMetrics(SM_CYSCREEN);
#else
            return 0;
#endif
        }

        // ── 设备层：硬件直读轮询(转发到 KeyMapper 的 IsKeyDown/IsMouseDown) ──

        bool Input::IsKeyDown(KeyCode key)
        {
            auto *mapper = GetMapper();
            return mapper->IsKeyDown(static_cast<uint32_t>(key));
        }

        bool Input::IsMouseDown(MouseCode button)
        {
            auto *mapper = GetMapper();
            return mapper->IsMouseDown(static_cast<uint32_t>(button));
        }

    } // namespace Input_t
} // namespace X_Y
