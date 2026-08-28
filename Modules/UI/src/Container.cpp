#include "Container/Container.h"
#include "DockLayout/DockLayout.h"
#include "UI/dock/Dock.h"
#include "Movement/MouseMovement.h"
#include "Widget/Application.h"

namespace X_Y
{

    Container::Container(XWidget *parent)
        : XWidget(parent)
    {
        // 布局 resize → 同步 DockLayout 尺寸并重排
        Connect(this, MovementType::WindowResize, this,
                [this](const XMovement &)
                { OnWindowResize(); });

        // 鼠标按下：优先让 DockLayout 拖分割线；未拦截则转发给激活 Panel
        Connect(this, MovementType::MouseButtonPressed, this,
                [this](const XMovement &e)
                {
            int lx = 0, ly = 0;
            GetMouseScreenPos(lx, ly);
            ScreenToClient(lx, ly);
            if (m_Layout)
            {
                if (!m_Layout->OnMousePressed(lx, ly))
                {
                    // 放行：转发给激活面板（内容交互）
                    Panel *p = m_Layout->GetActivePanel();
                    if (p)
                        p->DispatchMousePressed(lx - p->GetX(), ly - p->GetY());
                }
                CaptureMouse();
            } });

        // 鼠标移动
        Connect(this, MovementType::MouseMoved, this,
                [this](const XMovement &)
                {
            int lx = 0, ly = 0;
            GetMouseScreenPos(lx, ly);
            ScreenToClient(lx, ly);
            if (m_Layout)
            {
                if (!m_Layout->OnMouseMoved(lx, ly))
                {
                    Panel *p = m_Layout->GetActivePanel();
                    if (p)
                        p->DispatchMouseMoved(lx - p->GetX(), ly - p->GetY());
                }
            } });

        // 鼠标抬起
        Connect(this, MovementType::MouseButtonReleased, this,
                [this](const XMovement &)
                {
            int lx = 0, ly = 0;
            GetMouseScreenPos(lx, ly);
            ScreenToClient(lx, ly);
            if (m_Layout)
            {
                m_Layout->OnMouseReleased(lx, ly);
                Panel *p = m_Layout->GetActivePanel();
                if (p)
                    p->DispatchMouseReleased(lx - p->GetX(), ly - p->GetY());
            }
            ReleaseMouseCapture(); });
    }

    Container::~Container()
    {
        disConnect(this);
    }

    void Container::EnsureLayout()
    {
        if (m_Layout)
            return;
        m_Layout = new DockLayout();
        m_OwnLayout = true;
        m_Layout->SetHostRepaint([this]()
                                 { RequestRepaint(); });
    }

    void Container::SetDockLayout(DockLayout *layout)
    {
        // 若此前有自建布局，先交出
        if (m_OwnLayout)
        {
            delete m_Layout;
            m_OwnLayout = false;
        }
        m_Layout = layout;
        if (m_Layout)
            m_Layout->SetHostRepaint([this]()
                                     { RequestRepaint(); });
        if (GetNativeHandle())
        {
            m_Layout->SetActiveSize(static_cast<int>(get_width()),
                                    static_cast<int>(get_height()));
        }
        RequestRepaint();
    }

    Panel *Container::AddSinglePanel(Panel *panel, const std::string &title)
    {
        EnsureLayout();
        // 若布局还没有任何 Dock，懒建一个占满的 Dock
        Dock *dock = nullptr;
        if (!m_Layout->GetDockList().empty())
        {
            // 已有 dock，直接往最近一个加 tab；这里取第一个，后续可扩展
            dock = m_Layout->GetDockList().front();
        }
        if (!dock)
        {
            dock = new Dock();
            m_Layout->AddDock(dock);
            dock->SetHostRepaint([this]()
                                 { RequestRepaint(); });
            // 占满布局
            m_Layout->DockBind(*dock, InvalidBoundary, InvalidBoundary,
                               InvalidBoundary, InvalidBoundary);
        }
        return dock->AddPanel(panel, title);
    }

    Panel *Container::DetachPanel(Panel *panel)
    {
        if (!m_Layout)
            return nullptr;
        for (Dock *dock : m_Layout->GetDockList())
        {
            if (dock && dock->RemovePanel(panel))
            {
                RequestRepaint();
                return panel;
            }
        }
        return nullptr;
    }

    void Container::OnWindowResize()
    {
        if (m_Layout)
        {
            m_Layout->SetActiveSize(static_cast<int>(get_width()),
                                    static_cast<int>(get_height()));
        }
    }

    void Container::OnPaint(Canvas *canvas)
    {
        if (!canvas)
            return;
        if (m_Layout)
        {
            m_Layout->OnPaint(*canvas);
            return;
        }
        // 无内容：清默认背景
        canvas->Clear(0xFF202124);
    }

} // namespace X_Y
