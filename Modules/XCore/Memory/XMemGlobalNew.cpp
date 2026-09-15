// ═════════════════════════════════════════════════════════════════════════════
//  XMemGlobalNew.cpp — 全局 operator new / delete 重载（通道 ①：必经之路）
//
//  作用：让【所有】new（含 STL 容器、make_unique、第三方库）都经过门面，
//        从而做到"统计所有分配"+"没人能绕过内存体系"。
//
//  ── 为什么默认是安全的 ──
//    门面的默认后端就是 CRT（::malloc/::free），所以这套重载的效果是：
//        "标准库堆 + 一层记账"
//    而不是"换掉标准库的分配器"。因此：
//      · 性能与原生的差异 ≈ 一次函数调用 + 每次分配 16 字节分配头
//      · 行为对使用者完全透明（他们照常写 new，一切正常）
//      · 想优化时改门面策略即可，不用动任何调用点
//
//  ── 自举铁律（本文件尤其重要）──
//    1. 门面（Memory::Instance）必须是平凡对象，否则本文件第一行 new
//       就可能在它构造前触发 → UB。详见 XMemFacade.h 设计要点 2。
//    2. 后端/统计器/策略存储内部只用 ::malloc / ::free，绝不回调门面。
//       否则第一次 new → 门面 → 后端 → new → 死循环。
//    3. 本文件【不要】在重载内部使用任何会分配的东西
//       （std::string / std::vector / 打印格式化等都要小心）。
//       fprintf(stderr, ...) 本身不分配，可用。
//
//  ── 开关 ──
//    Memory::Instance().setEnabled(false) → 完全绕过门面，走原生
//    （出问题时的唯一退路，务必保留）
//
//  ── 覆盖范围与边界 ──
//    ✅ new / new[] / delete / delete[] / nothrow 版本 / sized delete
//    ✅ 所有 STL 容器、make_unique、make_shared（控制块也走 operator new）
//    ❌ 裸 malloc / calloc / realloc（需要系统钩子，风险大，不做）
//    ❌ VirtualAlloc 等直接系统调用
//    → 即"覆盖所有 C++ new，不含 C 的裸 malloc"
// ═════════════════════════════════════════════════════════════════════════════

#include "XMemFacade.h"

#include <cstdio>
#include <cstdlib>
#include <new>

namespace
{
    // 门面是否可用 / 是否启用。
    // ⚠️ 这里刻意【不用】门面的任何成员，只读一个原子开关，
    //    保证即使门面尚未就绪（静态初始化期）也不会崩。
    inline bool FacadeActive()
    {
        return X_Y::Memory::Instance().enabled();
    }

    // 分配失败：按门面的 OOMAction 语义上报，然后抛 bad_alloc（标准行为）
    [[noreturn]] inline void ThrowBadAlloc(std::size_t size)
    {
        std::fprintf(stderr,
                     "[XMem] allocation failed: %llu bytes\n",
                     (unsigned long long)size);
        throw std::bad_alloc();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  new / new[]
// ═════════════════════════════════════════════════════════════════════════════

void *operator new(std::size_t size)
{
    if (!FacadeActive())
    {
        // 退路：走原生（开关关闭时行为与没装这套完全一致）
        if (void *p = std::malloc(size ? size : 1))
            return p;
        ThrowBadAlloc(size);
    }

    if (void *p = X_Y::Memory::Instance().allocate(size))
        return p;

    // 门面返回 nullptr → 这里必须抛 bad_alloc（C++ 标准要求 new 失败抛异常）
    ThrowBadAlloc(size);
}

void *operator new[](std::size_t size)
{
    if (!FacadeActive())
    {
        if (void *p = std::malloc(size ? size : 1))
            return p;
        ThrowBadAlloc(size);
    }

    if (void *p = X_Y::Memory::Instance().allocate(size))
        return p;

    ThrowBadAlloc(size);
}

// ── nothrow 版本：失败返回 nullptr，不抛异常 ──
void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    if (!FacadeActive())
        return std::malloc(size ? size : 1);

    return X_Y::Memory::Instance().allocate(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    if (!FacadeActive())
        return std::malloc(size ? size : 1);

    return X_Y::Memory::Instance().allocate(size);
}

// ═════════════════════════════════════════════════════════════════════════════
//  delete / delete[]
//
//  ★ 关键：释放【绝不能】依赖"当前开关状态"，否则会出现错配：
//      分配时开关=开（走门面），之后开关被关，释放时走 ::free
//      → 把门面指针交给 CRT 堆 → 崩。
//    所以 delete 一律交给门面，由门面读【分配头】判断归属：
//      · 有合法分配头  → 是门面发的 → 归还给对应后端
//      · 没有分配头    → 不是门面发的 → 交给 ::free
//    这正是 deallocate 里 magic 检查的作用（见 XMemFacade.h）。
//
//    这样开关就只是"要不要接管新分配"，不影响已分配内存的释放，
//    运行中途切换也安全。
// ═════════════════════════════════════════════════════════════════════════════

void operator delete(void *ptr) noexcept
{
    if (!ptr)
        return;
    // 无条件交给门面：它自己会判断这个指针是不是它发的
    X_Y::Memory::Instance().deallocate(ptr, 0);
}

void operator delete[](void *ptr) noexcept
{
    if (!ptr)
        return;
    X_Y::Memory::Instance().deallocate(ptr, 0);
}

// ── sized delete（C++14，编译器可能优先调用它）──
//   门面靠分配头就知道真实大小，这里的 size 参数仅作参考。
void operator delete(void *ptr, std::size_t size) noexcept
{
    (void)size;
    operator delete(ptr);
}

void operator delete[](void *ptr, std::size_t size) noexcept
{
    (void)size;
    operator delete[](ptr);
}

// ── nothrow delete ──
void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
    operator delete(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    operator delete[](ptr);
}
