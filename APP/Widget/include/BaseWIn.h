#pragma once
#include <string>
#include <memory>
#include "WindowImpl.h"
#include "Canvas.h"

namespace X_Y {

    typedef unsigned int uint;

    class BaseWin
    {
    public:
        BaseWin();
        virtual ~BaseWin();

        void* GetNativeHandle() const { return m_NativeHandle; }
        void SetNativeHandle(void* handle) { m_NativeHandle = handle; }

        uint GetActualWidth() const { return m_ActualWidth; }
        uint GetActualHeight() const { return m_ActualHeight; }
        void SetActualSize(uint width, uint height) {
            m_ActualWidth = width; m_ActualHeight = height;
        }

        // ── 跨平台工具方法 ──────────────────────────────
        void GetScreenRect(int& left, int& top, int& right, int& bottom) const;
        void ScreenToClient(int& x, int& y) const;
        void ClientToScreen(int& x, int& y) const;
        void CaptureMouse();
        void ReleaseMouseCapture();
        void* GetParentNativeHandle() const;
        bool SetParent(void* newParent);

        void SetCursorStyle(CursorStyle style);
        void MoveAndResize(int x, int y, int w, int h, bool noZOrder = true);

        static void GetMouseScreenPos(int& x, int& y);
        static BaseWin* GetWindowAt(int screenX, int screenY);
        virtual void OnPaint(Canvas* canvas) {}  // 平台绘制回调

    protected:
        bool Show(ShowCmd nshow = ShowCmd::Show);
        void Close();
        void SetTitle(const char* title);
        void Destroy();
        virtual bool Create(const char* title, uint width, uint height,
                            WindowStyleFlag style = WindowStyleFlag::Overlapped,
                            void* parentHandle = nullptr);
        virtual std::string toString() const { return "BaseWindow"; }

    private:
        void* m_NativeHandle = nullptr;
        uint m_ActualWidth = 0;
        uint m_ActualHeight = 0;
        std::unique_ptr<WindowImpl> m_Impl;
    };

}
