#include "../Container/Container.h"
#include "../DockLayout/DockLayout.h"
#include "UI/dock/Dock.h"
#include "../UiCore/UIEvent.h"
#include "Movement/MouseMovement.h"
#include "Movement/KeyMovement.h"
#include "Widget/Application.h"
#include <cstdio>

namespace X_Y
{

    namespace
    {
        void TraceUI(const char *kind, int x, int y)
        {
            std::fprintf(stderr, "[UI] %s logical=(%d,%d)\n", kind, x, y);
        }
    }

    // 取相对本窗口客户区的逻辑坐标（命中路由统一用它）
    void GetClientPos(Container &self, int &x, int &y)
    {
        self.GetMouseScreenPos(x, y);
        self.ScreenToClient(x, y);
    }

    Container::Container(XWidget *parent)
        : XWidget(parent)
    {
        // 文件拖拽是窗口行为；Container 负责开启，具体面板决定如何处理。
        EnableFileDrop(true);

        // 布局 resize → 同步 DockLayout 尺寸并重排
        Connect(this, MovementType::WindowResize, this,
                [this](const XMovement &)
                { OnWindowResize(); });

        // 鼠标按下 → UIEvent(Press) → 布局路由（可能拖分割线/喂激活面板）
        Connect(this, MovementType::MouseButtonPressed, this,
                [this](const XMovement &e)
                {
            const auto &mb = dynamic_cast<const MouseButtonPressed &>(e);
            UIMouseEvent uie;
            uie.action = MouseAction::Press;
            uie.button = static_cast<Input_t::MouseCode>(mb.GetMouseButton());
            uie.x = static_cast<int>(mb.GetX());
            uie.y = static_cast<int>(mb.GetY());
            ClientPhysicalToLogical(uie.x, uie.y);
            TraceUI("mouse-press", uie.x, uie.y);
            if (m_Layout)
            {
                m_Layout->RouteInput(uie);
                CaptureMouse();
            } });

        // 鼠标移动 → UIEvent(Move)
        Connect(this, MovementType::MouseMoved, this,
                [this](const XMovement &e)
                {
            const auto &mm = dynamic_cast<const MouseMoved &>(e);
            UIMouseEvent uie;
            uie.action = MouseAction::Move;
            uie.x = static_cast<int>(mm.GetX());
            uie.y = static_cast<int>(mm.GetY());
            ClientPhysicalToLogical(uie.x, uie.y);
            TraceUI("mouse-release", uie.x, uie.y);
            if (m_Layout)
                m_Layout->RouteInput(uie); });

        // 鼠标抬起 → UIEvent(Release)
        Connect(this, MovementType::MouseButtonReleased, this,
                [this](const XMovement &e)
                {
            const auto &mb = dynamic_cast<const MouseButtonReleased &>(e);
            UIMouseEvent uie;
            uie.action = MouseAction::Release;
            uie.x = static_cast<int>(mb.GetX());
            uie.y = static_cast<int>(mb.GetY());
            ClientPhysicalToLogical(uie.x, uie.y);
            TraceUI("mouse-scroll", uie.x, uie.y);
            if (m_Layout)
                m_Layout->RouteInput(uie);
            ReleaseMouseCapture(); });

        // 滚轮 → UIEvent(Scroll)
        Connect(this, MovementType::MouseScrolled, this,
                [this](const XMovement &e)
                {
            const auto &ms = dynamic_cast<const MouseScrolled &>(e);
            UIMouseEvent uie;
            uie.action = MouseAction::Scroll;
            uie.scrollDelta = (int)ms.GetYOffset();
            uie.x = static_cast<int>(ms.GetX());
            uie.y = static_cast<int>(ms.GetY());
            ClientPhysicalToLogical(uie.x, uie.y);
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
        if (m_Layout && GetNativeHandle())
        {
            m_Layout->SetActiveSize(static_cast<int>(get_width()),
                                    static_cast<int>(get_height()));
        }
        RequestRepaint();
    }

    Panel *Container::AddSinglePanel(Panel *panel, const std::string &title)
    {
        if (m_Layout)
            return nullptr; // 复杂场景：已有布局，不能再塞单面板

        EnsureLayout();
        Dock *dock = CreateSinglePanelDock();
        if (!dock)
            return nullptr;
        dock->SetMaxPanelCount(1); // 单面板
        m_Layout->AddDock(dock);
        // 占满布局
        m_Layout->DockBind(*dock, InvalidBoundary, InvalidBoundary,
                           InvalidBoundary, InvalidBoundary);
        return dock->AddPanel(panel, title);
    }

    Dock *Container::CreateSinglePanelDock()
    {
        return new Dock();
    }

    Panel *Container::DetachPanel(Panel *panel, std::string *title)
    {
        if (!m_Layout)
            return nullptr;
        for (Dock *dock : m_Layout->GetDockList())
        {
            if (dock && dock->DetachPanel(panel, title))
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

    void Container::OnFileDragEnter(const std::vector<XPath> &files, int x, int y)
    {
        if (m_Layout)
            m_Layout->RouteFileDragEnter(files, x, y);
    }

    void Container::OnFileDragOver(const std::vector<XPath> &files, int x, int y)
    {
        if (m_Layout)
            m_Layout->RouteFileDragOver(files, x, y);
    }

    void Container::OnFileDragLeave()
    {
        if (m_Layout)
            m_Layout->RouteFileDragLeave();
    }

    void Container::OnFileDrop(const std::vector<XPath> &files, int x, int y)
    {
        if (m_Layout)
            m_Layout->RouteFileDrop(files, x, y);
    }

} // namespace X_Y
