#pragma once
#include "Widget/Canvas.h"
#include "UiCore/UIEvent.h"
#include "XCore/FilesSystem/FilesSystem.h"
#include <functional>
namespace X_Y
{

    // 统一的 UI 叶子元素（无 HWND，画到父级给的那张 Canvas）。
    // 事件只经一个入口 OnInput(UIInputEvent&)，不逐类堆散方法。
    class Component
    {
    public:
        Component() = default;
        virtual ~Component() = default;

        // ── 布局 ──
        void SetRect(int x, int y, int w, int h)
        {
            m_X = x;
            m_Y = y;
            m_W = w;
            m_H = h;
        }
        int GetX() const { return m_X; }
        int GetY() const { return m_Y; }
        int GetWidth() const { return m_W; }
        int GetHeight() const { return m_H; }

        void SetVisible(bool v) { m_Visible = v; }
        bool IsVisible() const { return m_Visible; }

        void SetFocused(bool f) { m_Focused = f; }
        bool IsFocused() const { return m_Focused; }

        // ── 输入（唯一入口）──
        // e.x/e.y 为相对本组件局部坐标；处理完设 e.Handled=true 即"吞掉"（停止冒泡）。
        // 基类默认空：不处理则不要动 Handled，让事件继续冒泡。
        virtual void OnInput(UIInputEvent &e) { (void)e; }

        // ── 滚动相关（内容自治度量，供 ScrollArea 布局）──
        virtual int GetScrollStep() const { return 1; }
        virtual void SetViewport(int scrollOffset, int viewHeight)
        {
            (void)scrollOffset;
            (void)viewHeight;
        }

        // ── 文件拖拽（独立通道：窗口级，不进输入路由）──
        virtual void OnFileDragEnter(const std::vector<XPath> &files, int localX, int localY) { (void)files; (void)localX; (void)localY; }
        virtual void OnFileDragOver(const std::vector<XPath> &files, int localX, int localY) { (void)files; (void)localX; (void)localY; }
        virtual void OnFileDragLeave() {}
        virtual void OnFileDrop(const std::vector<XPath> &files, int localX, int localY) { (void)files; (void)localX; (void)localY; }

        // 请求所属窗口重绘（由 Panel 在 AddComponent 时注入实现）
        void RequestRepaint()
        {
            if (m_RepaintCallback)
                m_RepaintCallback();
        }
        void SetRepaintCallback(std::function<void()> cb) { m_RepaintCallback = std::move(cb); }

        // 组件绘制自己（画到父级给的 canvas）
        virtual void OnPaint(Canvas &canvas) = 0;

    private:
        int m_X = 0, m_Y = 0, m_W = 100, m_H = 30;
        bool m_Visible = true;
        bool m_Focused = false;
        std::function<void()> m_RepaintCallback;
    };

}
