#include "Dock/Dock.h"

#include <algorithm>

namespace X_Y
{

    struct Dock::LayoutNode
    {
        enum class Kind
        {
            Panel,
            Split
        };

        Kind kind = Kind::Panel;
        DockPanel *panel = nullptr;
        DockSide side = DockSide::Right;
        float ratio = 0.5f;
        std::unique_ptr<LayoutNode> first;
        std::unique_ptr<LayoutNode> second;
    };

    Dock::Dock(XWidget *parent)
        : XWidget(parent)
    {
        SetWindowStyle(WindowStyleFlag::Child | WindowStyleFlag::Visible |
                       WindowStyleFlag::ClipChildren | WindowStyleFlag::ClipSiblings);
        if (parent && parent->GetNativeHandle())
            SetParentHwnd(parent->GetNativeHandle());

        m_Layout = std::make_unique<LayoutNode>();
        m_Layout->panel = CreatePanel();
    }

    Dock::~Dock()
    {
        DestroyPanels(m_Layout);
    }

    DockPanel *Dock::CreatePanel()
    {
        auto *panel = new DockPanel(this);
        if (GetNativeHandle())
        {
            panel->SetParentHwnd(GetNativeHandle());
            panel->setSize(GetActualWidth(), GetActualHeight());
            panel->show(ShowCmd::Show);
        }
        return panel;
    }

    DockPanel *Dock::GetRootPanel() const
    {
        LayoutNode *node = m_Layout.get();
        while (node && node->kind == LayoutNode::Kind::Split)
            node = node->first.get();
        return node ? node->panel : nullptr;
    }

    float Dock::ClampRatio(float ratio)
    {
        return std::clamp(ratio, 0.05f, 0.95f);
    }

    bool Dock::Contains(const LayoutNode *node, const DockPanel *panel)
    {
        if (!node)
            return false;
        if (node->kind == LayoutNode::Kind::Panel)
            return node->panel == panel;
        return Contains(node->first.get(), panel) || Contains(node->second.get(), panel);
    }

    void Dock::DestroyPanels(std::unique_ptr<LayoutNode> &node)
    {
        if (!node)
            return;
        if (node->kind == LayoutNode::Kind::Panel)
        {
            delete node->panel;
            node->panel = nullptr;
            return;
        }
        DestroyPanels(node->first);
        DestroyPanels(node->second);
    }

    bool Dock::SplitNode(LayoutNode *node, DockPanel *source, DockSide side,
                         float newRatio, DockPanel *newPanel)
    {
        if (!node)
            return false;
        if (node->kind == LayoutNode::Kind::Panel)
        {
            if (node->panel != source)
                return false;

            auto oldPanel = std::make_unique<LayoutNode>();
            oldPanel->panel = node->panel;
            auto addedPanel = std::make_unique<LayoutNode>();
            addedPanel->panel = newPanel;

            node->kind = LayoutNode::Kind::Split;
            node->side = side;
            node->ratio = ClampRatio(newRatio);
            if (side == DockSide::Left || side == DockSide::Top)
            {
                node->first = std::move(addedPanel);
                node->second = std::move(oldPanel);
            }
            else
            {
                node->first = std::move(oldPanel);
                node->second = std::move(addedPanel);
            }
            return true;
        }

        return SplitNode(node->first.get(), source, side, newRatio, newPanel) ||
               SplitNode(node->second.get(), source, side, newRatio, newPanel);
    }

    DockPanel *Dock::Split(DockPanel *source, DockSide side, float newRatio)
    {
        if (!m_AllowSplit || !source || !Contains(m_Layout.get(), source))
            return nullptr;

        auto *newPanel = CreatePanel();
        if (!SplitNode(m_Layout.get(), source, side, newRatio, newPanel))
        {
            delete newPanel;
            return nullptr;
        }
        RecalcLayout();
        return newPanel;
    }

    bool Dock::MergeNode(std::unique_ptr<LayoutNode> &node,
                         DockPanel *source, DockPanel *target)
    {
        if (!node || node->kind == LayoutNode::Kind::Panel)
            return false;

        const bool firstPair = node->first->kind == LayoutNode::Kind::Panel &&
                               node->second->kind == LayoutNode::Kind::Panel &&
                               node->first->panel == source && node->second->panel == target;
        const bool secondPair = node->first->kind == LayoutNode::Kind::Panel &&
                                node->second->kind == LayoutNode::Kind::Panel &&
                                node->first->panel == target && node->second->panel == source;
        if (firstPair || secondPair)
        {
            while (source->GetTabCount() > 0)
            {
                auto *tab = source->GetTab(0);
                const std::string title = tab ? tab->Title : std::string();
                auto *panel = source->ExtractPanel(tab);
                target->AddPanel(panel, title);
            }
            node = firstPair ? std::move(node->second) : std::move(node->first);
            return true;
        }

        return MergeNode(node->first, source, target) || MergeNode(node->second, source, target);
    }

    bool Dock::Merge(DockPanel *source, DockPanel *target)
    {
        if (!source || !target || source == target ||
            !Contains(m_Layout.get(), source) || !Contains(m_Layout.get(), target))
            return false;
        if (!MergeNode(m_Layout, source, target))
            return false;
        delete source;
        RecalcLayout();
        return true;
    }

    bool Dock::RemoveNode(std::unique_ptr<LayoutNode> &node, DockPanel *panel)
    {
        if (!node || node->kind == LayoutNode::Kind::Panel)
            return false;

        if (node->first->kind == LayoutNode::Kind::Panel && node->first->panel == panel)
        {
            node = std::move(node->second);
            return true;
        }
        if (node->second->kind == LayoutNode::Kind::Panel && node->second->panel == panel)
        {
            node = std::move(node->first);
            return true;
        }
        return RemoveNode(node->first, panel) || RemoveNode(node->second, panel);
    }

    bool Dock::RemovePanel(DockPanel *panel)
    {
        if (!panel || (m_Layout && m_Layout->kind == LayoutNode::Kind::Panel))
            return false;
        if (!RemoveNode(m_Layout, panel))
            return false;
        delete panel;
        RecalcLayout();
        return true;
    }

    void Dock::LayoutNodeChildren(LayoutNode *node, int x, int y, int w, int h)
    {
        if (!node)
            return;
        if (node->kind == LayoutNode::Kind::Panel)
        {
            node->panel->MoveAndResize(x, y, w, h);
            node->panel->show(ShowCmd::Show);
            return;
        }

        const bool horizontal = node->side == DockSide::Left || node->side == DockSide::Right;
        const bool firstBefore = node->side == DockSide::Left || node->side == DockSide::Top;
        const int firstSize = static_cast<int>((horizontal ? w : h) * node->ratio);
        const int secondSize = (horizontal ? w : h) - firstSize;

        if (firstBefore)
        {
            LayoutNodeChildren(node->first.get(), x, y,
                               horizontal ? firstSize : w,
                               horizontal ? h : firstSize);
            LayoutNodeChildren(node->second.get(),
                               horizontal ? x + firstSize : x,
                               horizontal ? y : y + firstSize,
                               horizontal ? secondSize : w,
                               horizontal ? h : secondSize);
        }
        else
        {
            LayoutNodeChildren(node->first.get(), x, y,
                               horizontal ? secondSize : w,
                               horizontal ? h : secondSize);
            LayoutNodeChildren(node->second.get(),
                               horizontal ? x + secondSize : x,
                               horizontal ? y : y + secondSize,
                               horizontal ? firstSize : w,
                               horizontal ? h : firstSize);
        }
    }

    void Dock::RecalcLayout()
    {
        const int w = static_cast<int>(get_width());
        const int h = static_cast<int>(get_height());
        if (w > 0 && h > 0)
            LayoutNodeChildren(m_Layout.get(), 0, 0, w, h);
    }

    void Dock::OnPaint(Canvas *canvas)
    {
        canvas->FillRect(0, 0, static_cast<int>(get_width()), static_cast<int>(get_height()), 0x282828);
    }

}