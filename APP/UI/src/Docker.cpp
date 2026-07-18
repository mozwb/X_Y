#include "dock/Docker.h"
#include "dock/DockLayer.h"
#include "Application/include/Application.h"

namespace X_Y {

// ════════════════════════════════════════════════════════════
// Docker
// ════════════════════════════════════════════════════════════

Docker::Docker(const std::string& title, int w, int h, XWidget* parent)
    : Container(parent)
{
    m_Panels[(int)Left]   = CreateScope<DockPanel>(this);
    m_Panels[(int)Right]  = CreateScope<DockPanel>(this);
    m_Panels[(int)Top]    = CreateScope<DockPanel>(this);
    m_Panels[(int)Bottom] = CreateScope<DockPanel>(this);
    m_Panels[(int)Center] = CreateScope<DockPanel>(this);

    m_Layer = new DockLayer(this);
    Application::instance()->PushLayer(m_Layer);

    setTitle(title.c_str());
    setSize(w, h);
}

Docker::~Docker()
{
    if (m_Layer) {
        Application::instance()->PopLayer(m_Layer);
        delete m_Layer;
        m_Layer = nullptr;
    }
    disConnect(this);
}

void Docker::Dock(XWidget* panel, const std::string& title, Area area)
{
    if (panel->getParent()) {
        panel->releaseSelf();
    }
    Panel(area)->AddPanel(panel, title);
    RecalcLayout();
}

void Docker::RecalcLayout()
{
    int w = get_width();
    int h = get_height();
    if (w <= 0 || h <= 0) return;

    auto topPn    = Panel(Top);
    auto bottomPn = Panel(Bottom);
    auto leftPn   = Panel(Left);
    auto rightPn  = Panel(Right);
    auto centerPn = Panel(Center);

    int topH = 0, bottomH = 0, leftW = 0, rightW = 0;

    if (topPn && !topPn->IsEmpty()) {
        topH = (int)(h * 0.2f);
        if (topH < 40) topH = 40;
        topPn->setSize(w, topH);
        topPn->MoveAndResize(0, 0, w, topH);
        topPn->show(SHOW);
    } else if (topPn) {
        topPn->show(HIDE);
    }

    if (bottomPn && !bottomPn->IsEmpty()) {
        bottomH = (int)(h * 0.2f);
        if (bottomH < 40) bottomH = 40;
        bottomPn->setSize(w, bottomH);
        bottomPn->MoveAndResize(0, h - bottomH, w, bottomH);
        bottomPn->show(SHOW);
    } else if (bottomPn) {
        bottomPn->show(HIDE);
    }

    int innerY = topH;
    int innerH = h - topH - bottomH;
    if (innerH < 0) innerH = 0;

    if (leftPn && !leftPn->IsEmpty()) {
        leftW = (int)(w * 0.2f);
        if (leftW < 60) leftW = 60;
        leftPn->setSize(leftW, innerH);
        leftPn->MoveAndResize(0, innerY, leftW, innerH);
        leftPn->show(SHOW);
    } else if (leftPn) {
        leftPn->show(HIDE);
    }

    if (rightPn && !rightPn->IsEmpty()) {
        rightW = (int)(w * 0.2f);
        if (rightW < 60) rightW = 60;
        rightPn->setSize(rightW, innerH);
        rightPn->MoveAndResize(w - rightW, innerY, rightW, innerH);
        rightPn->show(SHOW);
    } else if (rightPn) {
        rightPn->show(HIDE);
    }

    if (centerPn) {
        int cw = w - leftW - rightW;
        if (cw < 100) cw = 100;
        centerPn->setSize(cw, innerH);
        centerPn->MoveAndResize(leftW, innerY, cw, innerH);
        centerPn->show(SHOW);
    }
}

Docker::Area Docker::HitTestArea(int clientX, int clientY) const
{
    int w = GetActualWidth();
    int h = GetActualHeight();
    if (w <= 0 || h <= 0) return Center;

    const float EDGE = 0.15f;

    if (clientY < h * EDGE && Panel(Top) && !Panel(Top)->IsEmpty())
        return Top;
    if (clientY > h * (1.0f - EDGE) && Panel(Bottom) && !Panel(Bottom)->IsEmpty())
        return Bottom;
    if (clientX < w * EDGE && Panel(Left) && !Panel(Left)->IsEmpty())
        return Left;
    if (clientX > w * (1.0f - EDGE) && Panel(Right) && !Panel(Right)->IsEmpty())
        return Right;

    return Center;
}

} // namespace X_Y
