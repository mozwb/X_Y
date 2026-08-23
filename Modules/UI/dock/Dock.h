#pragma once

#include "UI/Panel/DockPanel.h"
#include "Widget/XWidget.h"
#include <memory>

namespace X_Y
{

    enum class DockSide
    {
        Left,
        Right,
        Top,
        Bottom
    };

    class Dock : public XWidget
    {
    public:
        explicit Dock(XWidget *parent = nullptr);
        ~Dock() override;

        DockPanel *GetRootPanel() const;
        DockPanel *Split(DockPanel *source, DockSide side, float newRatio = 0.5f);
        bool Merge(DockPanel *source, DockPanel *target);
        bool RemovePanel(DockPanel *panel);

        void SetAllowSplit(bool enabled) { m_AllowSplit = enabled; }
        bool IsSplitAllowed() const { return m_AllowSplit; }
        void RecalcLayout();

    protected:
        void OnPaint(Canvas *canvas) override;

    private:
        struct LayoutNode;

        std::unique_ptr<LayoutNode> m_Layout;
        bool m_AllowSplit = true;

        DockPanel *CreatePanel();
        static float ClampRatio(float ratio);
        static bool Contains(const LayoutNode *node, const DockPanel *panel);
        static void DestroyPanels(std::unique_ptr<LayoutNode> &node);
        bool SplitNode(LayoutNode *node, DockPanel *source, DockSide side,
                       float newRatio, DockPanel *newPanel);
        bool MergeNode(std::unique_ptr<LayoutNode> &node,
                       DockPanel *source, DockPanel *target);
        static bool RemoveNode(std::unique_ptr<LayoutNode> &node, DockPanel *panel);
        static void LayoutNodeChildren(LayoutNode *node, int x, int y, int w, int h);
    };

}
