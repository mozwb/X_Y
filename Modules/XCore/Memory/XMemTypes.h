#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  XMemTypes.h — 内存模块公共类型（最底层，谁都可以 include）
//
//  只放"大家都要用的枚举/小结构"，不放任何实现。
//  依赖方向：XMemTypes ← XMemBackend ← XMemStats ← XMemFacade
//            （单向，永不反向，避免循环依赖）
//
//  ⚠️ 本文件必须保持零依赖、零分配，它会在自举期被引用。
// ═════════════════════════════════════════════════════════════════════════════

#include <cstdint>

namespace X_Y
{

    // ── 后端类型标签 ─────────────────────────────────────────────────────────
    // 用于 allocate(size, type) 选择后端，也是"哪个后端"的可读名字。
    // 数组下标含义：统计器按本枚举值分槽记账（见 XMemStats.h）。
    enum class BackendType : uint8_t
    {
        Default = 0, // 走门面策略决定的后端（策略为空时 = Crt）
        Crt = 1,     // 标准库堆（malloc/free）—— 默认后端，标准库兜底
        Slab = 2,    // 自研 slab 分配器（已接入；仅显式指定或策略选用）
        Count        // 哨兵：统计槽位数
    };

    inline const char *BackendName(BackendType t)
    {
        switch (t)
        {
        case BackendType::Default:
            return "Default";
        case BackendType::Crt:
            return "Crt";
        case BackendType::Slab:
            return "Slab";
        default:
            return "Unknown";
        }
    }

    // ── OOM 处理策略 ─────────────────────────────────────────────────────────
    // "分配不到内存怎么办"属于分配层策略，门面与后端都要引用，故放最底层。
    enum class OOMAction
    {
        Abort,      // 开发期：超限直接 terminate，尽早暴露问题
        ReturnNull, // 发布版：超限返回 nullptr，调用方自己处理
        Expand,     // 调试期：突破预算继续分配，在统计中标记为超限
    };

    // ── 大小档（统计用）──────────────────────────────────────────────────────
    // ⚠️ 这不是分配器的分档，纯粹是【统计维度】：
    //    记录"程序在各尺寸区间分配了多少次"。
    //    用途：日后定分配策略时的数据依据 ——
    //    先看清程序到底在分配什么尺寸，才能定出有意义的分档策略。
    //
    //    Slab 分配器自己的档位（64B..64KB）在 SlabBackend 里，与本表无关。
    enum class SizeClass : uint8_t
    {
        Tiny = 0,    // ≤ 64B
        Small = 1,   // ≤ 256B
        Medium = 2,  // ≤ 1KB
        Large = 3,   // ≤ 4KB
        Huge = 4,    // ≤ 64KB
        Giant = 5,   // > 64KB
        Count
    };

    inline const char *SizeClassName(SizeClass c)
    {
        switch (c)
        {
        case SizeClass::Tiny:
            return "<=64B";
        case SizeClass::Small:
            return "<=256B";
        case SizeClass::Medium:
            return "<=1KB";
        case SizeClass::Large:
            return "<=4KB";
        case SizeClass::Huge:
            return "<=64KB";
        case SizeClass::Giant:
            return ">64KB";
        default:
            return "?";
        }
    }

    inline SizeClass ClassifySize(uint64_t size)
    {
        if (size <= 64)
            return SizeClass::Tiny;
        if (size <= 256)
            return SizeClass::Small;
        if (size <= 1024)
            return SizeClass::Medium;
        if (size <= 4096)
            return SizeClass::Large;
        if (size <= 65536)
            return SizeClass::Huge;
        return SizeClass::Giant;
    }

} // namespace X_Y
