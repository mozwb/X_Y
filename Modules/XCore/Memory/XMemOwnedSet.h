#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  XMemOwnedSet.h — 门面自管的"已分配指针"集合（声明）
//
//  用途：让 deallocate 能安全回答"这个指针是不是我发的"。
//
//  为什么需要它（关键）：
//    全局 operator delete 会把手边【所有】指针都送进门面，其中混有非门面
//    分配的（CRT 的、开关关闭期间分配的）。要判断归属，
//    ⚠️【不能】去读 ptr - kHeaderSize —— 外部指针那里可能是未映射内存，
//       甚至正好压在页边界上 → 直接段错误。
//    所以只能"先查表"，表里有才去读头。
//
//  ⚠️ 自举铁律：本结构【只用 ::malloc / ::free】，
//     绝不用 STL 容器、绝不用门面自己的 allocate —— 否则递归自举。
//     （这也是它不能用 std::unordered_set 的原因）
//
//  实现要点：开放寻址哈希表（线性探测），容量按 2 的幂增长；
//            删除用墓碑标记（否则线性探测会断链）。
//            详见 XMemOwnedSet.cpp
//
//  ⚠️ 线程安全：当前【无锁】。单线程没问题；
//     若出现高频多线程分配，需要分片（shard）或加锁。
// ═════════════════════════════════════════════════════════════════════════════

#include <cstddef>
#include <cstdint>

namespace X_Y
{

    class OwnedSet
    {
    public:
        OwnedSet() = default;
        ~OwnedSet();

        OwnedSet(const OwnedSet &) = delete;
        OwnedSet &operator=(const OwnedSet &) = delete;

        // 查表：ptr 是否在册（= 是门面发的）
        bool contains(void *ptr) const;

        // 登记。返回 false 表示扩容失败（内存不足），调用方应回滚本次分配。
        bool insert(void *ptr);

        // 摘除。返回 false 表示本来就不在册。
        bool erase(void *ptr);

        // 在册元素数（不含墓碑）
        std::size_t size() const { return m_Count; }

        // 表的当前容量（2 的幂；0 表示还没建过）
        std::size_t capacity() const { return m_Cap; }

    private:
        // 墓碑标记：用一个不可能作为合法用户指针的值。
        static void *Tomb();

        static std::size_t Hash(void *p);

        bool InsertNoGrow(void *ptr);
        bool Grow();

        void **m_Slots = nullptr;
        std::size_t m_Cap = 0;   // 2 的幂
        std::size_t m_Count = 0; // 有效元素数（不含墓碑）
    };

} // namespace X_Y
