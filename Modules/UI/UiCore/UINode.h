#pragma once

#include "Rect.h"
#include "../Component/Component.h"

namespace X_Y
{

    class DockLayout;
    class Dock;
    class Panel;

    // ============================================================
    // UINodeView — UI 四层统一的「节点视图」（绘制链与路由链共用的唯一描述）
    //
    // 背景（2026-09 坐标体系统一）：
    //   窗口布局侧（DockLayout/Dock）用【绝对坐标】，面板内侧（Panel/Component）
    //   用【相对坐标】。两套设计各自合理，但过去没有明确的转换点，导致
    //   "绘制画在哪"和"点击命中哪"各写一遍、还打架（非顶部 Dock 的 Panel 错位）。
    //
    // 定案：
    //   - 转换点落在 Dock↔Panel 交界，两侧各自纯粹。
    //   - 每个可绘制/可命中的层只声明三样：自己的矩形、内容矩形、可见性。
    //   - 绘制链和路由链【共用同一套坐标原语】（ToLocal/ToParent/Hits），
    //     于是两者结构同构，不可能再打架。
    //
    // ★ 核心约定：**self 一律位于【父坐标系】**。
    //     DockLayout 无 self（它是坐标系根）；
    //     Dock.self  在 DockLayout 坐标系（= 布局绝对）；
    //     Panel.self 在 Dock 坐标系（= Dock 局部！不再是绝对）；
    //     Component.self 在 Panel 坐标系（= Panel 局部）。
    //
    // ★ 现状（2026-09-12）：四层已全部改用本契约，不再是"预留"：
    //     DockLayout::OnPaint / RouteInput / HitTestDock   → View(Dock) + Hits/ToLocal/ToParent
    //     Dock::OnPaint / RouteInput / HitTestPanel        → View(Dock).content + ToLocal/ToParent
    //     Panel::OnPaint / OnInput / HitTest               → View(Panel)/View(Component) + 原语
    //   每层的"位置值"只在一处（各自的 View），绘制与路由都从它派生。
    //
    // ⚠️ 加新层/新节点时：只需提供 View() 重载 + 用这三个原语，
    //    不要在绘制或路由里另写一份坐标算术。
    //
    // 统一后组件作者与 Panel 作者都不需要感知宿主：
    //     void MyWidget::OnPaint(Canvas& c) override {
    //         c.FillRect(0, 0, GetWidth(), GetHeight(), 0xFF1E1E1E);  // 就这么多
    //     }
    // ============================================================
    struct UINodeView
    {
        Rect self;    // 本节点在【父坐标系】里的矩形
        Rect content; // 内容区（在【自身局部坐标系】里）：子节点绘制/命中的有效范围
                      // 例如 Dock 要避开顶部 tab 栏 → content = {0, tabH, w, h - tabH}
                      // ⚠️ 必须是【唯一真相】：content 要直接引用该层实际用于布局
                      //    子节点的那个矩形（Dock 用 m_EffPanel*、Panel 用整个自身），
                      //    不能"另算一份"。过去正是多份数据各算各的导致画的和点的打架。
        bool visible = true;

        // 便捷：内容区在父坐标系里的矩形（self 原点 + content 偏移）
        Rect ContentInParent() const
        {
            return Rect{ self.x + content.x, self.y + content.y, content.w, content.h };
        }

        // 便捷：内容区在自身局部坐标里的矩形（= content，语义别名，便于书写）
        Rect Content() const { return content; }
    };

    // ── 坐标原语（绘制链与路由链共用，只有这三个）──

    // 父坐标 → 本节点局部坐标
    inline void ToLocal(const UINodeView &v, int &x, int &y)
    {
        x -= v.self.x;
        y -= v.self.y;
    }

    // 本节点局部坐标 → 父坐标（下钻后还原，保证上层视角不被改）
    inline void ToParent(const UINodeView &v, int &x, int &y)
    {
        x += v.self.x;
        y += v.self.y;
    }

    // 命中：父坐标 (x,y) 是否落在本节点的内容区里
    inline bool Hits(const UINodeView &v, int x, int y)
    {
        return v.ContentInParent().Contains(x, y);
    }

    // ── 四层各自的 View() 重载 ──
    // 放在各层 .cpp 里定义（需完整类型），此处仅声明。

    UINodeView View(const Dock &node);
    UINodeView View(const Panel &node);
    UINodeView View(const Component &node);

} // namespace X_Y
