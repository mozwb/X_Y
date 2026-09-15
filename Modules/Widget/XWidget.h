#pragma once
#include "Movement/movements.h"
#include "Movement/AppMovement.h"
#include "Application.h"
#include "BaseWin.h"
#include <string>
#include <GraphicsContext/GraphicsContext.h>
#include <Log/XYLog.h>

namespace X_Y {

    using Base = BaseWin;
    using MovementType = X_Y::MovementType;

    class XWidget : public Base {
    public:
        explicit XWidget(XWidget* parent = nullptr);
        ~XWidget() { disConnect(this); }

        // ── 窗口生命周期（带事件连接） ──────────────
        bool show(ShowCmd nShow = ShowCmd::Show);
        void destroy();
        void disconnectPa();
        void releaseSelf();

        // ── 渲染（可选 OpenGL） ──────────────────────
        void Render() {
            XY_CORE_ASSERT(m_Context, "上下文不能为空");
            if (!m_Context->IsCurrent())
                if (!m_Context->MakeCurrent())
                    XY_CORE_ASSERT(false, "MakeCurrent失败");
            this->onRender();
            m_Context->SwapBuffers();
        }
        virtual void onRender() {}

        void SwapBuffers() { if (m_Context) m_Context->SwapBuffers(); }

        GraphicsContext* get_context() { return m_Context.get(); }

        // 创建 OpenGL 上下文（通过工厂，隐藏平台细节）
        void createGraphicsContext(GraphicsType type = GraphicsType::OpenGL) {
            m_Context.reset(GraphicsContextFactory::Create(GetNativeHandle(), type));
            if (m_Context) m_Context->Init();
        }

        // ── 窗口属性 ────────────────────────────────
        bool create() {
            return this->Create(m_title, GetActualWidth(), GetActualHeight(),
                                m_WindowStyle, m_ParentHwnd);
        }
        void setTitle(const char* title);
        void setSize(uint width, uint height);
        uint get_width() const { return GetActualWidth(); }
        uint get_height() const { return GetActualHeight(); }
        std::string getname() { return toString(); }
        std::string toString() const override { return m_title; }

        XWidget* getParent() const { return m_parent; }
        void SetWindowStyle(WindowStyleFlag style) { m_WindowStyle = style; }
        void SetParentHwnd(void* hwnd) { m_ParentHwnd = hwnd; }

        XWidget(const XWidget&) = delete;
        XWidget& operator=(const XWidget&) = delete;

    protected:
        // ── 窗口原生销毁 → 对象退场（自毁）──
        // 由构造函数连到 MovementType::WindowDestroy（平台消息泵在 WM_DESTROY
        // 时发出）。⚠️ 语义放在 XWidget 而不是各平台实现里：
        //   将来接 X11 / Cocoa，只要它的消息泵也发 WindowDestroy，
        //   自毁自动生效，不用改任何平台代码。
        //
        // ⚠️ 不能当场 delete this：本函数跑在 dispatcher 的绑定遍历中，
        //    ProcessEvents 也还在栈上，都持有 this。所以只向 Application
        //    登记一个延迟回收动作，等本轮事件派发结束后的安全点执行。
        void OnNativeDestroyed();

    private:
        XWidget* m_parent = nullptr;
        Scope<GraphicsContext> m_Context;
        const char* m_title = "X_Y";
        WindowStyleFlag m_WindowStyle = WindowStyleFlag::None;
        void* m_ParentHwnd = nullptr;

        // ── 回收门闩：保证同一个对象只被 delete 一次 ──
        // WindowDestroy 只发一次，正常不会重复入队；但若将来有别的路径也调
        // OnNativeDestroyed，重复入队的动作会在 flush 时 delete 两遍 → 崩溃。
        // 这个标志位就是"我已经排过回收队了"的死亡证明。
        bool m_RecycleQueued = false;
    };

}
