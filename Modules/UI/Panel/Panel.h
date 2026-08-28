#pragma once
#include "Component/Component.h"
#include "Widget/Canvas.h"
#include <vector>
#include <functional>
#include <cstdint>

namespace X_Y
{

// ─────────────────────────────────────────────────────────────
// Panel — 纯逻辑内容单元（无 HWND、无 XWidget）
//
// Panel 是 UI 解耦后的最小自洽内容：一棵组件树 + 它占哪块矩形 + 怎么画 +
// 怎么把"宿主转来的输入"转发给组件。
// 它【完全不知道】宿主是谁（独立窗口 / Dock 面板 / 离屏），只认：
//   - 宿主给的一张 Canvas&             → OnPaint(canvas)
//   - 宿主给的布局矩形                 → SetLayoutRect(x,y,w,h)
//   - 宿主注入的重绘回调               → SetHostRepaint(cb)
//   - 宿主算好的相对本 Panel 的输入坐标 → DispatchMouse*(localX, localY) 等
//
// 宿主责任（三脚架）：
//   1. OnPaint 里调 panel->OnPaint(canvas)
//   2. 尺寸变化时 panel->SetLayoutRect(...)
//   3. 源 Input 收到 Movement → 换算成本 Panel 相对坐标 → 调下述 Dispatch*
//      （坐标换算、鼠标捕获、焦点等窗口级行为归宿主，Panel 不碰 HWND）
// ─────────────────────────────────────────────────────────────
class Panel
{
public:
    Panel() = default;
    virtual ~Panel() = default;

    Panel(const Panel &) = delete;
    Panel &operator=(const Panel &) = delete;

    // ── 布局：我这个面板占哪块矩形（宿主每次调整时调用）──
    void SetLayoutRect(int x, int y, int w, int h);
    void GetLayoutRect(int &x, int &y, int &w, int &h) const;
    int GetX() const { return m_X; }
    int GetY() const { return m_Y; }
    int GetWidth() const { return m_W; }
    int GetHeight() const { return m_H; }

    // ── 组件管理（沿用旧 Container 的接口，不重造）──
    void AddComponent(Component *comp);
    void RemoveComponent(Component *comp);
    void ClearComponents();
    std::vector<Component *> &Components() { return m_Components; }

    // ── 重绘请求：不调宿主窗口，而调注入的回调 ──
    void SetHostRepaint(std::function<void()> cb) { m_HostRepaint = std::move(cb); }
    void RequestRepaint() { if (m_HostRepaint) m_HostRepaint(); }

    // ── 绘制：宿主递给一张画布，我把我这整棵树画上去 ──
    virtual void OnPaint(Canvas &canvas);

    // ── 输入转发入口（坐标 = 相对本 Panel 左上角的逻辑坐标，由宿主换算好用）
    //    UI 私有事件系统上线前，用传参调用转发；届时不替换这套，改包一层。
    void DispatchMousePressed(int localX, int localY);
    void DispatchMouseMoved(int localX, int localY);
    void DispatchMouseReleased(int localX, int localY);
    void DispatchKeyDown(Input_t::KeyCode key);
    void DispatchChar(wchar_t ch);
    void DispatchMouseScrolled(int localX, int localY, float yDelta);

protected:
    // 命中测试：相对本 Panel 坐标(x,y) → 命中组件（z 序上→下，rbegin 反向）
    Component *HitTest(int x, int y);

    int m_X = 0, m_Y = 0, m_W = 100, m_H = 100;
    std::vector<Component *> m_Components;
    std::function<void()> m_HostRepaint;

    // 从按下到抬起的持续交互目标（拖动滑块等）
    Component *m_DragTarget = nullptr;
    bool m_DragTargetVisible = false;
};

} // namespace X_Y
