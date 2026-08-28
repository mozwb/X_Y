#pragma once
#include "Rect.h"
#include "UIEvent.h"
#include <vector>

namespace X_Y
{

    // ============================================================
    // 命中路由简化工具 —— 沉淀"每层薄命中 + 事件冒泡"的公共小逻辑。
    //
    // 说明：多级 UI 树跨不同类型（Dock→Panel→Component）做"一套模板自动递归"
    //       在 C++ 里做不到类型自动消歧，过度模板反而难读。
    //       务实做法：每层实现一个【极薄的】命中方法（非虚、几行），只做
    //       "矩形判断 + 调下一层"；薄壳共用 Rect 工具 + 事件对象（带 Handled 冒泡）。
    //       这里只沉淀 Rect 判定 + 冒泡分发两个最小公共函数，减少重复。
    // ============================================================

    // 事件冒泡分发的最小循环：
    //   给定当前节点列表（z 序，索引越大越上层），找命中者并喂事件。
    //   e 共享一个对象，处理者设 Handled=true 即终止（吞掉）。
    //   返回是否被处理（供上层决定是否继续往父级冒）。
    template <typename ObjT>
    bool RouteToHit(std::vector<ObjT> &objs, int x, int y, UIInputEvent &e)
    {
        // z 序：反向遍历（后加的/上面的优先命中）
        for (auto it = objs.rbegin(); it != objs.rend(); ++it)
        {
            ObjT o = *it;
            Rect r = o->Rect();
            if (r.Contains(x, y))
            {
                // 转成子对象局部坐标再喂
                e.x = x - r.x;
                e.y = y - r.y;
                o->OnInput(e);
                if (e.Handled)
                    return true; // 吞掉，不再冒
                // 未吞：继续试下面的兄弟（同一层冒泡）
            }
        }
        return e.Handled;
    }

} // namespace X_Y
