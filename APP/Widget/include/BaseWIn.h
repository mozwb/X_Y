#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <functional>
#include "WindowImpl.h"
#include "Canvas.h"

namespace X_Y {

    typedef unsigned int uint;

    class BaseWin
    {
    public:
        BaseWin();
        virtual ~BaseWin();

        void* GetNativeHandle() const { return m_Impl ? m_Impl->GetNativeHandle() : nullptr; }

        uint GetActualWidth() const { return m_ActualWidth; }       // 逻辑宽
        uint GetActualHeight() const { return m_ActualHeight; }     // 逻辑高
        // 物理宽/高（真实像素，供系统级功能如 DockLayer 停靠）
        uint GetActualWidthPhysical() const {
            return m_Impl ? m_Impl->GetClientWidthPhysical() : m_ActualWidth;
        }
        uint GetActualHeightPhysical() const {
            return m_Impl ? m_Impl->GetClientHeightPhysical() : m_ActualHeight;
        }
        void SetActualSize(uint width, uint height) {   // 逻辑宽高
            m_ActualWidth = width; m_ActualHeight = height;
        }

        // ── 跨平台工具方法 ──────────────────────────────
        // ⚠️ 坐标约定（重要，改代码前先看）：
        //   默认方法（无后缀）返回/接收【逻辑坐标】，供 UI 布局/绘制/命中测试用。
        //   需要真实像素的地方（如 DockLayer 停靠区判定、跨窗口坐标）
        //   用带 Physical 后缀的方法。上层不用自己乘/除 scale。
        void GetScreenRect(int& left, int& top, int& right, int& bottom) const;
        void ScreenToClient(int& x, int& y) const;          // 物理屏幕→逻辑客户区
        void ClientToScreen(int& x, int& y) const;          // 逻辑客户区→物理屏幕
        void ScreenToClientPhysical(int& x, int& y) const;  // 物理屏幕→物理客户区
        void ClientToScreenPhysical(int& x, int& y) const;  // 物理客户区→物理屏幕
        void GetClientRectPhysical(int& l, int& t, int& r, int& b) const; // 物理客户区
        void CaptureMouse();
        void ReleaseMouseCapture();
        void* GetParentNativeHandle() const;
        bool SetParent(void* newParent);

        void SetCursorStyle(CursorStyle style);
        void MoveAndResize(int x, int y, int w, int h, bool noZOrder = true);

        void RequestRepaint();
        void ValidateWindow();
        void PaintDirect(std::function<void(Canvas&)> painter);

        // 主线程跳过此窗口的 WM_PAINT（用于独立线程自绘）
        std::atomic<bool> m_SkipMainThreadPaint{ false };

        static void GetMouseScreenPos(int& x, int& y);
        static BaseWin* GetWindowAt(int screenX, int screenY);
        virtual void OnPaint(Canvas* canvas) {}  // 平台绘制回调

        // ── 自定义拖拽状态（由 WndProc 维护） ──────────
        bool m_IsDragging = false;
        int m_DragOffsetX = 0;
        int m_DragOffsetY = 0;

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
        uint m_ActualWidth = 0;
        uint m_ActualHeight = 0;
        std::unique_ptr<WindowImpl> m_Impl;
    };

}
