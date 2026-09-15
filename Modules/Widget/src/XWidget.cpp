#include "XWidget.h"
#include "Log/XYLog.h"

namespace X_Y
{

    XWidget::XWidget(XWidget *parent)
        : m_parent(parent)
    {
        XDEBUG("XWidget 构造执行,parent={}", (uintptr_t)parent)
        auto app = Application::instance();
        if (!app)
        {
            XY_CORE_ASSERT(false, "Application instance is null!");
            return;
        }

        if (m_parent)
        {
            XDEBUG("子窗口构造执行")
            connect(this, MovementType::WindowClose, this, &XWidget::destroy);
            connect(parent, MovementType::WindowClose, this, &XWidget::destroy);
        }
        else if (!app->IsFirstWin())
        {
            app->updateFirstWin();
            connect(this, MovementType::WindowClose, app, &Application::appClose);
        }
        else
        {
            connect(this, MovementType::WindowClose, this, &XWidget::destroy);
        }

        Connect(this, MovementType::WindowResize, this, [this](const XMovement &e)
                {
        auto& resize = dynamic_cast<const WindowResize&>(e);
        XDEBUG("窗口resize: {}x{}", resize.GetWidth(), resize.GetHeight()); });

        // ── 原生销毁 → 对象自毁（跨平台语义，不写在 Win32 里）──
        // 平台消息泵在 WM_DESTROY 时发 WindowDestroy（见 Win32WndProc.cpp），
        // 这里接住它，把"这个 C++ 对象该退场了"这件事交给 Application 的
        // 延迟回收安全点 —— 不能当场 delete，理由见 OnNativeDestroyed 注释。
        connect(this, MovementType::WindowDestroy, this, &XWidget::OnNativeDestroyed);
    }

    // ── 窗口原生销毁 → 登记延迟回收 ──
    //
    // 调用时机：WM_DESTROY 之后，dispatcher 正在遍历绑定表把 WindowDestroy
    // 送过来。此刻 ProcessEvents / DispatchEvent 全都还在栈上且持有 this，
    // 直接 delete this 会立刻 迭代器失效 + UAF。
    //
    // ⇒ 只入队，等 ProcessEvents 一轮结束后的安全点由 Application 真正释放。
    void XWidget::OnNativeDestroyed()
    {
        // 门闩：已经排过队就不再排（防 flush 时 delete 两遍）
        if (m_RecycleQueued)
            return;
        m_RecycleQueued = true;

        Application *app = Application::instance();
        if (!app)
        {
            // 没有 Application（异常场景，如已析构）→ 退回直接删除。
            // 此时不在 dispatcher 遍历中，相对安全。
            delete this;
            return;
        }

        // ── 死前退订 ──
        // 目的：对象马上要被 delete，dispatcher 里不能残留任何指向它的绑定，
        //       否则后续派发会打到野指针。
        //
        // 两个重载比的字段不同（见 movements.h:143 / :164）：
        //   双参 disConnect(sender, receiver) —— 比 sender && receiver
        //   单参 disConnect(receiver)         —— 只比 receiver
        //
        // 实测（不是推测）：
        //   · 构造函数里注册的 `connect(this, type, this, ...)` 是 sender==receiver==this，
        //     单参版【本来就能删掉】它（receiver 字段就是 this）。
        //   · 但会漏掉 sender==this && receiver!=this 的那类绑定
        //     （如"本窗口发事件给别的对象处理"）。双参版专治这条。
        // ⇒ 两条各删各的，合起来才彻底。留两条是有意的，不是冗余。
        // ⚠️ MovementSender / MovementReceiver 都是 void* 同一类型，
        //    `disConnect(self, self)` 唯一匹配双参重载，不歧义（已实测）。
        void *self = this;
        disConnect(self, self); // sender == this && receiver == this
        disConnect(self);       // 所有 receiver == this（含 sender 是别人的）

        app->DeferRecycle([this]()
                          { delete this; });
    }

    bool XWidget::show(ShowCmd nShow)
    {
        if (BaseWin::Show(nShow))
            return true;
        XINFO("窗口未创建，自动创建窗口: {}", m_title);
        if (this->Create(m_title, GetActualWidth(), GetActualHeight(),
                         m_WindowStyle, m_ParentHwnd))
        {
            return this->show(nShow);
        }
        XERROR("窗口创建失败")
        return false;
    }

    void XWidget::setTitle(const char *title)
    {
        m_title = title;
        this->SetTitle(title);
    }

    void XWidget::setSize(uint width, uint height)
    {
        SetActualSize(width, height);
    }

    void XWidget::destroy()
    {
        disConnect(this);
        this->Destroy();
    }

    void XWidget::disconnectPa()
    {
        if (m_parent)
        {
            disConnect(m_parent, this);
        }
    }

    void XWidget::releaseSelf()
    {
        disconnectPa();
        m_parent = nullptr;
    }

}
