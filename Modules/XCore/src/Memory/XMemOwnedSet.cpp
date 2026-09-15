// ═════════════════════════════════════════════════════════════════════════════
//  XMemOwnedSet.cpp — 已分配指针表的实现
//
//  ⚠️ 自举铁律（本文件尤其重要）：
//     本文件【只允许】使用 ::malloc / ::calloc / ::free。
//     绝不能用 new / std::vector / std::unordered_set：
//       它们会走全局 operator new → 回调门面 → 门面查本表 → 递归死循环。
//
//  性能：一次分配 = 一次插入；一次释放 = 一次查找。
//        负载因子 0.7 时平均探测 < 3 次，可接受。
//        扩容是 O(n) 重哈希，但按 2 倍增长摊还后是 O(1)。
// ═════════════════════════════════════════════════════════════════════════════

#include "../../Memory/XMemOwnedSet.h"

#include <cstdlib>

namespace X_Y
{

    // 墓碑标记：用一个不可能作为合法用户指针的值。
    // ⚠️ 不能写成 constexpr（C++ 不允许整数→指针的常量表达式），
    //    故用函数返回；运行期会被常量折叠，无开销。
    void *OwnedSet::Tomb()
    {
        return reinterpret_cast<void *>(static_cast<std::uintptr_t>(1));
    }

    OwnedSet::~OwnedSet()
    {
        std::free(m_Slots);
        m_Slots = nullptr;
        m_Cap = 0;
        m_Count = 0;
    }

    // 指针通常对齐到 16 字节，低位信息少 → 先右移再混淆（splitmix64 变体）
    std::size_t OwnedSet::Hash(void *p)
    {
        std::size_t x = reinterpret_cast<std::size_t>(p) >> 4;
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        return x;
    }

    bool OwnedSet::contains(void *ptr) const
    {
        if (!m_Slots || m_Count == 0 || !ptr)
            return false;

        std::size_t i = Hash(ptr) & (m_Cap - 1);
        for (std::size_t probe = 0; probe < m_Cap; ++probe)
        {
            void *slot = m_Slots[i];
            if (slot == nullptr)
                return false; // 空位 = 探测链结束（线性探测的终止条件）
            if (slot == ptr)
                return true;
            i = (i + 1) & (m_Cap - 1);
        }
        return false;
    }

    bool OwnedSet::insert(void *ptr)
    {
        if (!ptr)
            return false;

        // 负载因子到 0.7 → 扩容（容量翻倍 + 重哈希）
        if (m_Cap == 0 || (m_Count + 1) * 10 >= m_Cap * 7)
        {
            if (!Grow())
                return false;
        }
        return InsertNoGrow(ptr);
    }

    bool OwnedSet::erase(void *ptr)
    {
        if (!m_Slots || !ptr)
            return false;

        std::size_t i = Hash(ptr) & (m_Cap - 1);
        for (std::size_t probe = 0; probe < m_Cap; ++probe)
        {
            void *slot = m_Slots[i];
            if (slot == nullptr)
                return false;
            if (slot == ptr)
            {
                // ★ 必须用墓碑而不是 nullptr：直接把槽位清空会截断
                //   其他元素的探测链，导致它们查不到。
                m_Slots[i] = Tomb();
                --m_Count;
                return true;
            }
            i = (i + 1) & (m_Cap - 1);
        }
        return false;
    }

    // 假定容量足够，直接插入。
    // 复用第一个遇到的墓碑位（若有），否则用空位。
    bool OwnedSet::InsertNoGrow(void *ptr)
    {
        std::size_t i = Hash(ptr) & (m_Cap - 1);
        std::size_t firstTomb = SIZE_MAX;

        for (std::size_t probe = 0; probe < m_Cap; ++probe)
        {
            void *slot = m_Slots[i];
            if (slot == ptr)
                return true; // 已存在
            if (slot == Tomb() && firstTomb == SIZE_MAX)
                firstTomb = i;
            else if (slot == nullptr)
            {
                const std::size_t target = (firstTomb != SIZE_MAX) ? firstTomb : i;
                m_Slots[target] = ptr;
                ++m_Count;
                return true;
            }
            i = (i + 1) & (m_Cap - 1);
        }

        // 全表皆墓碑（理论上不该发生，因为负载因子会先触发扩容）
        if (firstTomb != SIZE_MAX)
        {
            m_Slots[firstTomb] = ptr;
            ++m_Count;
            return true;
        }
        return false;
    }

    bool OwnedSet::Grow()
    {
        const std::size_t newCap = m_Cap ? m_Cap * 2 : 1024;
        void **newSlots = static_cast<void **>(std::calloc(newCap, sizeof(void *)));
        if (!newSlots)
            return false;

        void **oldSlots = m_Slots;
        const std::size_t oldCap = m_Cap;

        m_Slots = newSlots;
        m_Cap = newCap;
        m_Count = 0;

        // 把旧表里的有效元素重新插入（跳过空位与墓碑）
        for (std::size_t i = 0; i < oldCap; ++i)
        {
            void *p = oldSlots[i];
            if (p && p != Tomb())
                InsertNoGrow(p);
        }

        std::free(oldSlots);
        return true;
    }

} // namespace X_Y
