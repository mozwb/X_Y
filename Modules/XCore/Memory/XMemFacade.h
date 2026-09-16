#pragma once

// ═════════════════════════════════════════════════════════════════════════════
//  XMemFacade.h — 内存门面（Facade）
//
//  这是外界【唯一】要认识的文件。分工：
//      XMemTypes.h       ← 公共枚举（BackendType / OOMAction / SizeClass）
//      XMemBackend.h     ← 后端：字节从哪来、怎么还
//      XMemStats.h       ← 统计：只记账
//      XMemFacade.h      ← 门面：外界入口，选后端、记账、归属判定 ★
//      XMemGlobalNew.cpp ← 全局 operator new/delete 重载（必经之路）
//
//  ── 门面的两条意义（这是整个模块存在的理由）──
//    ① 统计：所有分配都记账（按后端 / 按大小档 / 峰值 / 未释放块）
//    ② 策略：可注入"这次该走哪个后端"，从而优化内存分配
//    分配器（slab）只是"策略的一种实现"，真正有价值的是这层可插拔的
//    策略 + 统计 —— 优化只需改策略，全局受益。
//
//  ── 三条通道（各管各的场景）──
//    ① 全局 operator new 重载 → 统一兜底，没人能绕过（XMemGlobalNew.cpp）
//       使用者什么都不用改，new / STL / 第三方全部进门面。
//    ② X_Y::Malloc / X_Y::Free → 裸内存，替代 C 的 malloc/free
//    ③ X_Y::AllocFrom / NewFrom → 显式指定后端（单次，绕过策略）
//
//  ── 设计要点（改之前必读）──
//
//  1. 为什么必须有 header（分配头）：
//     `delete p` 时 C++ 【不会】把构造时的额外参数传回来，所以门面
//     无法知道 p 当初走的是哪个后端。因此在分配时把"我是谁"记在
//     返回指针前面，deallocate 时往回读一眼就知道该找谁。
//     这保证了：**策略/后端随便换，释放永不错配**。
//
//  2. 归属判定靠 header 验真，不靠"已分配指针表"、也不遍历后端：
//     全局 operator delete 会把手边【所有】指针送进来，其中混有非门面
//     分配的（CRT 的、开关关闭期间分配的）。要判断"是不是我发的"，
//     答案就在 ptr 前面的 header 里，三条一起看：
//       magic（是不是我写的）+ user（是不是正好等于 ptr）+ checksum（自洽）
//     三条全过 = 我发的；任一不过 = 不是我的 → 交回 ::free。
//     故释放是 O(1)：读 header → 按 backend 字段找后端 → 归还。
//     ⚠️ 曾用过哈希表（OwnedSet）登记每个在册指针，已【退役】：
//        header 里的字段本来就能回答这个问题，表是重复存储 + 每次分配/释放
//        的额外开销；旧实现（XMemory.h）也从来没有表。
//     ⚠️ 也【曾】加过"先问各后端 own() 判断是不是自家页"的快路径，同样
//        已删除：它是净负债 —— 让每次 delete 抢一把全局锁、变成 O(后端数)，
//        却没防住任何东西（三连验真照样要读同一块内存）。
//
//  3. 门面必须是平凡可初始化的：
//     全局 operator new 重载后，门面可能在静态初始化期（甚至更早）
//     被触达。故门面的成员都设计成"不需要运行期构造"的形态，
//     后端懒就位；策略是裸函数指针（见 AllocPolicy 的说明）。
//
//  4. 自举铁律：
//     后端、统计器、策略存储内部【只用 ::malloc / ::free】，
//     绝不回调门面。否则第一次 allocate 就会死循环。
//     ⚠️ 这条也约束"用户传进来的东西"：策略是函数指针而非 std::function，
//        就是为了杜绝"用户 lambda 的捕获物触发 operator new → 回调门面"。
//
//  ── 用法 ──
//      // 日常：直接 new 就行（全局重载接管，含 STL）
//      auto* w = new Widget(args...);
//      delete w;
//
//      // 裸内存：用这个，别用 C 的 malloc
//      void* p = X_Y::Malloc(1024);
//      X_Y::Free(p);
//
//      // 显式指定后端
//      void* q = X_Y::AllocFrom(BackendType::Slab, 1024);
//      auto* o = X_Y::NewFrom<BackendType::Slab, Widget>(args...);
//
//      // 策略：决定 new 走哪个后端
//      // ⚠️ 默认没有策略 = 全部走 Crt（标准库兜底）。
//      //    Slab 是【按需启用】的：只在确实有收益的热点上开，
//      //    效果不好把策略清掉（clearPolicy）就回到 Crt，不用改任何调用点。
//      //
//      // ⚠️ 策略必须是【无捕获的普通函数】（或静态成员函数）——
//      //    捕获 lambda 在这里【编译不过】，这是有意的：
//      //    std::function 的捕获物可能触发 operator new → 回调门面 → 自举递归。
//      //    详见上方 AllocPolicy 的说明。
//      static BackendType SmallToSlab(uint64_t n)
//      {
//          return n <= 512 ? BackendType::Slab : BackendType::Default;
//      }
//      Memory::Instance().setPolicy(&SmallToSlab);
//
//      // 需要"策略读配置"时：把配置做成门面的成员，
//      // 让上面这个静态函数去读它 —— 而不是让策略去捕获。
//
//      // 统计
//      Memory::Instance().markBaseline();   // main 开头调一次
//      Memory::Instance().stats();          // 全量快照
//      Memory::Instance().liveBlocks();     // 泄漏自检（应为 0）
//
//      // 旧名兼容（现有代码如 Buffer 零改动）
//      void* r = Memory::Instance().Alloc(1024);
//      Memory::Instance().Free(r);
// ═════════════════════════════════════════════════════════════════════════════

#include "XMemBackend.h"
#include "XMemStats.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <utility>

namespace X_Y
{

    // ── 分配头（AllocHeader）────────────────────────────────────────────────
    // 布局：  [ AllocHeader ][ 用户数据 ... ]
    //                        ↑ 返回给用户的地址
    //
    // 每个分配前面都藏一份，用来回答四件事：
    //   size     —— 原始请求大小（还给后端，并用于统计销账）
    //   backend  —— 该还给哪个后端
    //   user     —— 用户区地址（★ 验真用：见下）
    //   checksum —— ★ 验真用：由 size+backend+user 算出的自校验
    //   magic    —— 这个指针是不是我们发的？
    //
    //  ★ 为什么靠这两个新字段就够了（不再需要"已分配指针表"）：
    //    释放时门面拿到的只有 ptr，它必须回答"这是不是我发的"。
    //    旧方案维护一张哈希表逐个登记指针（每次分配插、释放删、扩容重哈希）；
    //    新方案把答案记在 ptr 前面，前提是"敢读 ptr - kHeaderSize"。
    //    敢不敢读由两步决定（见 XMemFacade.cpp 的 deallocate）：
    //      ① 地址落在已知自管区间内（问各后端 own()）→ 一定是自家页，必可读；
    //      ② 否则读 header 后做三连验真：magic 对 + user 自洽 + checksum 对。
    //    三连全过 = 门面发的；任一不过 = 不是门面发的 → 交回 ::free。
    //    ⚠️ 旧实现（XMemory.h 的 Free）本来就是这么干的 —— 它压根没有表。
    //
    // ⚠️ 对齐：header 必须占整数倍对齐字节，保证后面用户数据仍满足
    //    alignof(max_align_t)。故 kHeaderSize 向上取整到 16。
    //
    // ⚠️ 改动本结构 = 改动门面与释放路径的契约：加字段务必同步 Checksum()。
    struct AllocHeader
    {
        uint64_t size;     // 用户请求的字节数
        uint64_t checksum; // ★ 验真：Checksum(*this)
        uintptr_t user;    // ★ 验真：用户区地址（= raw + kHeaderSize）
        uint32_t magic;    // kAllocMagic 标记
        uint8_t backend;   // BackendType
        uint8_t pad[3];
    };

    inline constexpr uint32_t kAllocMagic = 0x58595F4D;         // 'X','Y','_','M'
    inline constexpr uint64_t kHeaderRaw = sizeof(AllocHeader); // 32
    inline constexpr uint64_t kHeaderSize =
        (kHeaderRaw + 15) & ~static_cast<uint64_t>(15);

    // ── 分配头自校验 ─────────────────────────────────────────────────────────
    // 由 size + backend + user 揉出来的一个值，写进 header->checksum。
    // 用途：捕获"内存被踩""指针被改""跨后端错配"这类 header 内部不自洽的情况。
    //
    // ⚠️ 它【不是】安全哈希，只求便宜 + 能把不自洽暴露出来：
    //    门面上还叠了 magic 与 user 自洽两道，三者一起才构成判决。
    //    所以这里不需要密码学强度，也就不引入任何依赖。
    inline uint64_t Checksum(const AllocHeader &h)
    {
        uint64_t x = h.size;
        x ^= static_cast<uint64_t>(h.backend) * 0x9E3779B97F4A7C15ULL;
        x ^= static_cast<uint64_t>(h.user) + 0x165667B19E3779F9ULL;
        x ^= 0xC2B2AE3D27D4EB4FULL; // 常量种子
        // splitmix64 收尾
        x ^= x >> 30;
        x *= 0xBF58476D1CE4E5B9ULL;
        x ^= x >> 27;
        x *= 0x94D049BB133111EBULL;
        x ^= x >> 31;
        return x;
    }

    // ⚠️ 归属判定没有独立的表：判据就是分配头里的字段（见 AllocHeader 与
    //    deallocate 实现）。所以本类不再持有 OwnedSet。

    // ── Memory：门面 ─────────────────────────────────────────────────────────
    class Memory
    {
    public:
        // ── 单例 ──
        // 门面成员都刻意做成"不需要运行期堆分配"的形态，因此
        // 首次取用时不会触发 allocate，不存在自举递归。
        static Memory &Instance()
        {
            static Memory s_Inst;
            return s_Inst;
        }

        // ═════════════════════════════════════════════════════════════════════
        //  配置
        // ═════════════════════════════════════════════════════════════════════
        void setDefaultBackend(BackendType t) { m_DefaultBackend = t; }
        BackendType defaultBackend() const { return m_DefaultBackend; }

        void setMaxBytes(uint64_t bytes) { s_MaxBytes = bytes; }
        uint64_t maxBytes() const { return s_MaxBytes; }

        void setOOMAction(OOMAction a) { s_OOMAction = a; }
        OOMAction oomAction() const { return s_OOMAction; }

        // ── 一键开关 ──
        // 关闭后新的分配完全走原生（::malloc），零干预。
        // ⚠️ 只影响【分配】；释放永远交门面自己判断归属，所以运行中
        //    切换开关是安全的（不会错配）。
        void setEnabled(bool on) { s_Enabled.store(on, std::memory_order_relaxed); }
        bool enabled() const { return s_Enabled.load(std::memory_order_relaxed); }

        // ═════════════════════════════════════════════════════════════════════
        //  策略：决定"这次该走哪个后端"
        // ═════════════════════════════════════════════════════════════════════
        // 输入：本次请求的字节数。输出：该走哪个后端。
        // 返回 Default 表示"没意见，用默认后端"。
        //
        // ⚠️ 策略【只影响分配】。释放永远读分配头里的记录，
        //    所以随时换策略都不会出现"分配走 A、释放走 B"的错配。
        //
        // ⚠️⚠️ 为什么是【裸函数指针】而不是 std::function（改这里前必读）：
        //    std::function 有个小缓冲（SBO），捕获物塞不下时它会调
        //    operator new —— 而本模块【重载了全局 operator new】，
        //    于是这条路径会绕回门面的 allocate，造成自举递归：
        //        用户调 setPolicy(捕获较大的 lambda)
        //          → std::function 构造 → operator new（全局重载）
        //          → Memory::allocate() → 而此刻 m_Policy 尚未就绪 → 递归/UB
        //    这在"首次分配之前"调用时尤其致命（静态初始化期）。
        //    手动 malloc + placement new 也【解决不了】——那只安排了
        //    std::function 对象本身在哪，捕获物照样在堆上。
        //    故策略只能是裸函数指针：无捕获、无状态、绝无堆分配可能。
        //    需要"策略读配置"时，把配置做成门面的成员 + 写个无捕获的
        //    静态函数去读它，而不是让策略去捕获。
        //    （这也把错误挡在编译期：捕获 lambda 会直接不匹配这个类型）
        using AllocPolicy = BackendType (*)(uint64_t);

        void setPolicy(AllocPolicy policy)
        {
            // 裸指针，直接原子存即可 —— 无需分配、无需析构。
            m_Policy.store(policy, std::memory_order_release);
        }

        void clearPolicy() { setPolicy(nullptr); }

        bool hasPolicy() const
        {
            return m_Policy.load(std::memory_order_acquire) != nullptr;
        }

        // ── 统计零点校准（"去皮"）──
        // 在 main 开头调一次：之后统计读取都相对此刻。
        // 目的：启动期的分配（全局对象、CRT 自身）不污染泄漏自检。
        // ⚠️ 只影响读取，不影响分配行为。
        void markBaseline() { m_Counter.markBaseline(); }
        void clearBaseline() { m_Counter.clearBaseline(); }
        bool hasBaseline() const { return m_Counter.hasBaseline(); }

        // ═════════════════════════════════════════════════════════════════════
        //  核心分配 / 释放（STL 风格命名）
        // ═════════════════════════════════════════════════════════════════════

        // 分配 size 字节。（实现见 XMemFacade.cpp）
        //   type == Default → 先问策略，策略没意见则用默认后端
        //   type == 具体后端 → 单次覆盖，绕过策略
        // 失败行为由 OOMAction 决定；Abort 之外统一返回 nullptr。
        void *allocate(uint64_t size, BackendType type = BackendType::Default);

        // 归还。★ 不需要传后端、不需要传大小 —— 全靠 header / 归属表。
        //
        // ⚠️ 为什么这里要容忍"非门面指针"（fallback 到 ::free）：
        //    全局 operator delete 重载后，delete 会把【所有】指针交到这里，
        //    包括 CRT 自己分配的、或开关关闭期间分配的。
        //    所以：是门面发的 → 读头归还；不是 → 交回 ::free。
        void deallocate(void *ptr, uint64_t size = 0);

        // 归属判定（自检，也可对外用于诊断）
        // 实现见 .cpp：读 ptr 前面的 header 做三连验真（magic + user + checksum）。
        // ⚠️ 它【不在】释放热路径上 —— 释放直接读 header，同样 O(1)。
        bool owns(void *ptr) const;

        // 后端自检：这个地址落在哪个后端的自管区域里？没有则返回 nullptr。
        // ⚠️ 与 owns() 不同，它【不读任何 header】，只问后端自己的区域表
        //    （Slab 的 chunk 区间查询）。这是 own() 接口的用途所在。
        // ⚠️ 它不在分配/释放的任何路径上，所以后端的 own() 可以做得
        //    "精确但偏慢"（Slab 要拿索引锁 + 二分）而不影响性能。
        IMemoryBackend *backendOwning(void *ptr);

        // ═════════════════════════════════════════════════════════════════════
        //  对象级便捷接口（分配 + 构造 / 析构 + 归还）
        // ═════════════════════════════════════════════════════════════════════

        template <typename T, typename... Args>
        T *Allocate(BackendType type, Args &&...args)
        {
            void *mem = allocate(sizeof(T), type);
            if (!mem)
                return nullptr;
            try
            {
                return new (mem) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                deallocate(mem, sizeof(T)); // 构造抛异常不泄漏
                throw;
            }
        }

        template <typename T, typename... Args>
        T *Allocate(Args &&...args)
        {
            return Allocate<T>(BackendType::Default, std::forward<Args>(args)...);
        }

        template <typename T>
        void Deallocate(T *ptr)
        {
            if (!ptr)
                return;
            ptr->~T();
            deallocate(ptr, sizeof(T));
        }

        // ═════════════════════════════════════════════════════════════════════
        //  旧名兼容层（现有调用点零改动：Buffer 等）
        // ═════════════════════════════════════════════════════════════════════
        void *Alloc(uint64_t size) { return allocate(size); }
        void Free(void *ptr) { deallocate(ptr, 0); }

        template <typename T, typename... Args>
        T *AllocT(Args &&...args)
        {
            return Allocate<T>(BackendType::Default, std::forward<Args>(args)...);
        }

        template <typename T>
        void FreeT(T *ptr) { Deallocate(ptr); }

        // ═════════════════════════════════════════════════════════════════════
        //  统计
        // ═════════════════════════════════════════════════════════════════════
        MemoryStats stats() const { return m_Counter.Snapshot(); }
        MemoryStats GetStats() const { return m_Counter.Snapshot(); } // 旧名兼容

        uint64_t UsedBytes() const { return m_Counter.UsedBytesRelative(); }
        uint64_t UsedCapacity() const { return m_Counter.PeakBytesRelative(); }
        uint64_t TotalAllocs() const { return m_Counter.TotalAllocs(); }
        uint64_t TotalFrees() const { return m_Counter.TotalFrees(); }
        uint64_t OOMCount() const { return m_Counter.OOMCount(); }

        // ★ 泄漏自检：还活着的块数。正常程序收尾时必须是 0。
        //   （调过 markBaseline 后是相对基线的，判据才准）
        uint64_t liveBlocks() const { return m_Counter.LiveBlocksRelative(); }
        bool hasLeaks() const { return liveBlocks() != 0; }

        // 基线之前发生了多少次分配（= 开机成本，不参与泄漏判断）
        uint64_t earlyAllocs() const { return m_Counter.EarlyAllocs(); }

        // 在册分配数（= 尚未释放的块数）。
        // ⚠️ 旧版这里读的是 OwnedSet 的元素个数；表退役后直接问统计器，
        //    语义不变（"在册"就等于"分配了还没释放"），且跨基线也不会跑偏。
        uint64_t ownedCount() const
        {
            return m_Counter.LiveBlocks();
        }

        bool UnderBudget() const
        {
            return s_MaxBytes == 0 || m_Counter.UsedBytes() < s_MaxBytes;
        }

        std::string ToString() const { return m_Counter.ToString(); }

        // ═════════════════════════════════════════════════════════════════════
        //  后端管理
        // ═════════════════════════════════════════════════════════════════════
        // 注册后端（不接管所有权，调用方自己保证生命周期长于使用期）。
        void registerBackend(BackendType t, IMemoryBackend *backend)
        {
            const uint32_t i = static_cast<uint32_t>(t);
            if (i < kBackendSlots)
                m_Backends[i].store(backend, std::memory_order_release);
        }

        bool hasBackend(BackendType t) const
        {
            const uint32_t i = static_cast<uint32_t>(t);
            if (i >= kBackendSlots)
                return false;
            if (t == BackendType::Crt)
                return true; // Crt 永远可用（静态兜底）
            return m_Backends[i].load(std::memory_order_acquire) != nullptr;
        }

        IMemoryBackend *backendFor(BackendType t);

        // 关闭（程序收尾调用；报告未回收块）。实现见 XMemFacade.cpp
        void Shutdown();

    private:
        Memory() = default;
        ~Memory() = default;
        Memory(const Memory &) = delete;
        Memory &operator=(const Memory &) = delete;

        // 解析"这次该走哪个后端"（带 size，让策略能按大小决策）
        BackendType ResolveWithSize(BackendType t, uint64_t size) const;

        static constexpr uint32_t kBackendSlots =
            static_cast<uint32_t>(BackendType::Count);

        // ⚠️ 成员都刻意保持"不需要运行期构造"的形态
        BackendType m_DefaultBackend = BackendType::Crt;
        bool m_Shutdown = false;
        std::atomic<IMemoryBackend *> m_Backends[kBackendSlots] = {};
        MemoryCounter m_Counter; // 自管（固定数组 + 原子），零堆分配

        // 已分配指针表已退役：归属判定改由 header 字段承担（见 AllocHeader）。
        // 这里曾经有个 OwnedSet m_OwnedTable —— 它是 per-pointer 的重复存储，
        // 而 header 里的 magic/user/checksum 本来就能回答同一个问题。

        // 策略：一个裸函数指针（见 AllocPolicy 的说明）。
        // ⚠️ 以前这里是 std::atomic<AllocPolicy*> —— 指向 malloc 出来的
        //    std::function。改成裸指针后不需要任何分配与析构，
        //    门面成员保持"零运行期构造"，静态初始化期绝对安全。
        std::atomic<AllocPolicy> m_Policy{nullptr};

        // 全局配置（inline static，零初始化）
        inline static uint64_t s_MaxBytes = 0; // 0 = 不限预算
        inline static OOMAction s_OOMAction = OOMAction::Abort;
        inline static std::atomic<bool> s_Enabled{true};
    };

    // ═════════════════════════════════════════════════════════════════════════
    //  通道 ②③：命名空间级便捷函数
    // ═════════════════════════════════════════════════════════════════════════

    // ── ② 裸内存：替代 C 的 malloc / free ──────────────────────────────────
    //
    // ⚠️⚠️ 重要警告（务必遵守）：
    //   1. 本函数分配的内存【只能用 X_Y::Free 或 delete 释放】，
    //      绝不能用 C 的 free()（那是另一个堆，会崩）。
    //   2. 反过来，C 的 malloc 分配的内存也不要交给 X_Y::Free
    //      （门面会识别出来并交回 ::free，但那是编程错误）。
    //   3. 这是【裸内存】—— 没有构造/析构。
    //      要构造对象请用 new 或 NewFrom。
    inline void *Malloc(uint64_t size)
    {
        return Memory::Instance().allocate(size);
    }

    // 释放裸内存。
    // ⚠️ 只用于 Malloc / MallocFrom / AllocFrom 返回的指针。
    //    不要用来释放 new 出来的【对象】（那会跳过析构 → 资源泄漏）。
    //    （对象请用 delete / Memory::Deallocate）
    inline void Free(void *ptr)
    {
        Memory::Instance().deallocate(ptr, 0);
    }

    // ── ③ 显式指定后端（单次，绕过策略）────────────────────────────────────
    inline void *MallocFrom(BackendType backend, uint64_t size)
    {
        return Memory::Instance().allocate(size, backend);
    }

    inline void *AllocFrom(BackendType backend, uint64_t size)
    {
        return Memory::Instance().allocate(size, backend);
    }

    // 带构造：后端放在模板参数里（编译期可知 → 可内联，零运行期开销）
    //   用法：auto* w = X_Y::NewFrom<BackendType::Slab, Widget>(args...);
    template <BackendType Backend, typename T, typename... Args>
    T *NewFrom(Args &&...args)
    {
        void *mem = Memory::Instance().allocate(sizeof(T), Backend);
        if (!mem)
            return nullptr;
        try
        {
            return new (mem) T(std::forward<Args>(args)...);
        }
        catch (...)
        {
            Memory::Instance().deallocate(mem, sizeof(T)); // 不泄漏
            throw;
        }
    }

    // 配套：析构 + 归还（与 NewFrom 成对，读代码时配对明确）
    template <typename T>
    void DeleteFrom(T *ptr)
    {
        Memory::Instance().Deallocate(ptr);
    }

} // namespace X_Y
