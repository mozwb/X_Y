#pragma once
#include "Movement/include/movements.h"
#include "Movement/include/AppMovement.h"
#include"Application/include/Application.h"
#include "BaseWin.h"
#include <string>
#include <GraphicsContext/include/GraphicsContext.h>
#include <Log/include/XYLog.h>

namespace X_Y {

    using Base = BaseWin;
    using MovementType = X_Y::MovementType;

    class XWidget : public Base {
    public:
        explicit XWidget(XWidget* parent = nullptr);
        ~XWidget() { destroy(); }

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

    private:
        XWidget* m_parent = nullptr;
        Scope<GraphicsContext> m_Context;
        const char* m_title = "X_Y";
        WindowStyleFlag m_WindowStyle = WindowStyleFlag::None;
        void* m_ParentHwnd = nullptr;
    };

}
