#include "Container/Container.h"
#include "DockLayout/DockLayout.h"
#include "UI/dock/Dock.h"
#include "UiCore/UIEvent.h"
#include "Movement/MouseMovement.h"
#include "Movement/KeyMovement.h"
#include "Widget/Application.h"

namespace X_Y
{

    // 取相对本窗口客户区的逻辑坐标（命中路由统一用它）
    void GetClientPos(Container &self, int &x, int &y)
    {
        self.GetMouseScreenPos(x, y);
        self.ScreenToClient(x, y);
    }

    Container::Container(XWidget *parent)
        : XWidget(parent)
    {
        // 布局 resize → 同步 DockLayout 尺寸并重排
        Connect(this, MovementType::WindowResize, this,
                [this](const XMovement &)
                { OnWindowResize(); });

        // 鼠标按下 → UIEvent(Press) → 布局路由（可能拖分割线/喂激活面板）
        Connect(this, MovementType::MouseButtonPressed, this,
                [this](const XMovement &e)
                {
            int lx = 0, ly = 0;
            GetClientPos(*this, lx, ly);
            const auto &mb = dynamic_cast<const MouseButtonPressed &>(e);
            UIMouseEvent uie;
            uie.action = MouseAction::Press;
            uie.button = static_cast<Input_t::MouseCode>(mb.GetMouseButton());
            uie.x = lx; uie.y = ly;
            if (m_Layout)
            {
                m_Layout->RouteInput(uie);
                CaptureMouse();
            } });

        // 鼠标移动 → UIEvent(Move)
        Connect(this, MovementType::MouseMoved, this,
                [this](const XMovement &)
                {
            int lx = 0, ly = 0;
            GetClientPos(*this, lx, ly);
            UIMouseEvent uie;
            uie.action = MouseAction::Move;
            uie.x = lx; uie.y = ly;
            if (m_Layout)
                m_Layout->RouteInput(uie); });

        // 鼠标抬起 → UIEvent(Release)
        Connect(this, MovementType::MouseButtonReleased, this,
                [this](const XMovement &)
                {
            int lx = 0, ly = 0;
            GetClientPos(*this, lx, ly);
            UIMouseEvent uie;
            uie.action = MouseAction::Release;
            uie.x = lx; uie.y = ly;
            if (m_Layout)
                m_Layout->RouteInput(uie);
            ReleaseMouseCapture(); });

        // 滚轮 → UIEvent(Scroll)
        Connect(this, MovementType::MouseScrolled, this,
                [this](const XMovement &e)
                {
            int lx = 0, ly = 0;
            GetClientPos(*this, lx, ly);
            const auto &ms = dynamic_cast<const MouseScrolled &>(e);
            UIMouseEvent uie;
            uie.action = MouseAction::Scroll;
            uie.scrollDelta = (int)ms.GetYOffset();
            uie.x = lx; uie.y = ly;
            if (m_Layout)
                m_Layout->RouteInput(uie); });

        // 按键 → UIKeyEvent(not char)
        Connect(this, MovementType::KeyPressed, this,
                [this](const XMovement &e)
                {
            const auto &kp = dynamic_cast<const KeyPressed &>(e);
            UIKeyEvent uie;
            uie.key = kp.GetKeyCode();
            uie.isChar = false;
            if (m_Layout)
                m_Layout->RouteInput(uie); });

        // 字符 → UIKeyEvent(char)
        Connect(this, MovementType::KeyTyped, this,
                [this](const XMovement &e)
                {
            const auto &kt = dynamic_cast<const KeyTyped &>(e);
            UIKeyEvent uie;
            uie.ch = (wchar_t)kt.GetKeyCode();
            uie.isChar = true;
            if (m_Layout)
                m_Layout->RouteInput(uie); });
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
