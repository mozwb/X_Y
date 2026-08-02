#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace X_Y {

// ─────────────────────────────────────────────────────────────────────────────
//  LoopQueue — 固定容量循环队列（FIFO，满则覆盖最旧）
//
//  定位：
//    - 通用基础设施，不只服务于某一场景
//    - 容量在编译期固定（模板参数 N），底层 std::array 连续内存
//    - 满时 Push 会覆盖最旧元素，头部随覆盖前移
//    - 全局序号单调递增，配合 At() 支持“按序号取最新 N 条”的增量消费
//
//  接口贴合 STL 容器习惯：
//    - Push / operator[] / Size / Capacity / Empty / Clear
//    - begin / end（支持范围 for 与 STL 算法）
//    - At(globalSeq) 增量访问
// ─────────────────────────────────────────────────────────────────────────────

template<class T, size_t N>
class LoopQueue {
    static_assert(N > 0, "LoopQueue requires N > 0");
public:
    using value_type      = T;
    using size_type       = size_t;
    using reference       = T&;
    using const_reference = const T&;

    
    // ── 迭代器（顺序 = 最旧 → 最新，正确处理环形绕回） ──
    
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        iterator(LoopQueue* owner, size_type idx)
            : m_Owner(owner), m_Index(idx) {}

        reference operator*()  const { return m_Owner->operator[](m_Index); }
        pointer   operator->() const { return &m_Owner->operator[](m_Index); }

        iterator& operator++() { ++m_Index; return *this; }
        iterator  operator++(int) { iterator tmp = *this; ++(*this); return tmp; }

        bool operator==(const iterator& o) const {
            return m_Owner == o.m_Owner && m_Index == o.m_Index;
        }
        bool operator!=(const iterator& o) const { return !(*this == o); }

    private:
        LoopQueue*  m_Owner;
        size_type   m_Index;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

        const_iterator(const LoopQueue* owner, size_type idx)
            : m_Owner(owner), m_Index(idx) {}

        reference operator*()  const { return m_Owner->operator[](m_Index); }
        pointer   operator->() const { return &m_Owner->operator[](m_Index); }

        const_iterator& operator++() { ++m_Index; return *this; }
        const_iterator  operator++(int) { const_iterator tmp = *this; ++(*this); return tmp; }

        bool operator==(const const_iterator& o) const {
            return m_Owner == o.m_Owner && m_Index == o.m_Index;
        }
        bool operator!=(const const_iterator& o) const { return !(*this == o); }

    private:
        const LoopQueue* m_Owner;
        size_type        m_Index;
    };

    iterator       begin()       { return iterator(this, 0); }
    iterator       end()         { return iterator(this, m_Count); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end()   const { return const_iterator(this, m_Count); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend()   const { return const_iterator(this, m_Count); }

    // ── 容量 ──

    bool     Empty() const { return m_Count == 0; }
    size_type Size() const { return m_Count; }
    size_type Capacity() const { return N; }
    bool     Full() const { return m_Count == N; }

    // ── 入队：满则覆盖最旧，返回落点引用 ──

    reference Push(const T& item) {
        size_t pos = (m_Head + m_Count) % N;
        if (m_Count >= N) {
            m_Data[pos] = item;
            m_Head = (m_Head + 1) % N;   // 覆盖最旧，头部前移
        } else {
            m_Data[pos] = item;
            m_Count++;
        }
        ++m_Total;
        return m_Data[pos];
    }

    reference Push(T&& item) {
        size_t pos = (m_Head + m_Count) % N;
        if (m_Count >= N) {
            m_Data[pos] = std::move(item);
            m_Head = (m_Head + 1) % N;
        } else {
            m_Data[pos] = std::move(item);
            m_Count++;
        }
        ++m_Total;
        return m_Data[pos];
    }

    // ── 相对访问：0 = 最旧，Size()-1 = 最新 ──

    reference operator[](size_type i) {
        return m_Data[(m_Head + i) % N];
    }
    const_reference operator[](size_type i) const {
        return m_Data[(m_Head + i) % N];
    }

    // ── 全局序号访问：i 为单调递增序号（TotalPushed 语义） ──
    //     仅保证“最新的 min(N, 已入队数) 条”有效，更老的下标可能指向已覆盖数据

    reference At(uint64_t globalSeq) {
        return m_Data[globalSeq % N];
    }
    const_reference At(uint64_t globalSeq) const {
        return m_Data[globalSeq % N];
    }

    
    // 最旧 / 最新

    reference       Front()       { return operator[](0); }
    const_reference Front() const { return operator[](0); }
    reference       Back()        { return operator[](m_Count > 0 ? m_Count - 1 : 0); }
    const_reference Back()  const { return operator[](m_Count > 0 ? m_Count - 1 : 0); }

    // ── 清除 ──
   
    void Clear() {
        m_Head = 0;
        m_Count = 0;
        // 注意：不清 m_Total（全局序号保持单调，避免重复消费）
    }

    // 完全重置（连同序号）
    void Reset() {
        m_Head = 0;
        m_Count = 0;
        m_Total = 0;
    }

    uint64_t TotalPushed() const { return m_Total; }

private:
    std::array<T, N> m_Data;
    size_t   m_Head = 0;    // 最旧元素下标
    size_t   m_Count = 0;   // 当前条数 ≤ N
    uint64_t m_Total = 0;   // 全局入队计数（单调递增）
};

} 
// namespace X_Y
