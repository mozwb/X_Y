// ═════════════════════════════════════════════════════════════════════════════
//  XMemFacade.cpp — 门面的核心分配/释放实现
//
//  为什么这些实现在 .cpp 而不是头文件里内联：
//    它们是门面的"重活"（分配头维护、预算检查、归属判定、后端分发），
//    篇幅大、细节多，放在头文件里会淹没门面的**接口**，读起来很累。
//    只有那些"必须在静态初始化期就能调用"的极小路（Instance / enabled /
//    命名空间级的 Malloc 等）才留在头文件内联。
//
//  ⚠️ 自举铁律（改本文件前必读）：
//     本文件不得使用任何会堆分配的东西：
//       std::string / std::vector / std::ostringstream / printf 的某些实现
//     因为它们会走全局 operator new → 回到门面 allocate → 递归。
//     fprintf(stderr, ...) 与 ::malloc / ::free 是安全的。
//     例外：ToString() 里的 ostringstream —— 它只在用户主动打印时调用，
//     那时门面早已就绪，且不在分配路径上。
// ═════════════════════════════════════════════════════════════════════════════

#include "XMemFacade.h"

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace X_Y
{

    // ═════════════════════════════════════════════════════════════════════════
    //  内部工具
    // ═════════════════════════════════════════════════════════════════════════

    // 解析"这次该走哪个后端"（带 size，让策略能按大小决策）
    BackendType Memory::ResolveWithSize(BackendType t, uint64_t size) const
    {
        // 显式指定 → 单次覆盖，绕过策略
        if (t != BackendType::Default && t != BackendType::Count)
            return t;

        AllocPolicy *policy = m_Policy.load(std::memory_order_acquire);
        if (policy && *policy)
        {
            const BackendType chosen = (*policy)(size);
            if (chosen != BackendType::Default && chosen != BackendType::Count)
                return chosen;
        }
        return m_DefaultBackend;
    }

    IMemoryBackend *Memory::backendFor(BackendType t)
    {
        const uint32_t i = static_cast<uint32_t>(t);
        if (i >= kBackendSlots)
            return nullptr;

        // Crt 后端是平凡类（只有 ::malloc/::free，无状态无构造），
        // 放在函数内 static：首次取用时已就绪，多线程安全。
        static CrtBackend s_Crt;

        if (t == BackendType::Crt && m_Backends[i].load(std::memory_order_acquire) == nullptr)
        {
            // 原子发布：多线程同时首次 allocate 时都写同一个值，无害。
            m_Backends[i].store(&s_Crt, std::memory_order_release);
        }
        return m_Backends[i].load(std::memory_order_acquire);
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  核心分配
    // ═════════════════════════════════════════════════════════════════════════

    void *Memory::allocate(uint64_t size, BackendType type)
    {
        if (size == 0)
            return nullptr;

        const BackendType resolved = ResolveWithSize(type, size);
        IMemoryBackend *backend = backendFor(resolved);
        if (!backend)
            return nullptr; // 该后端未接入（如 Slab 尚未迁移）

        // ── 预算检查 ──
        if (s_MaxBytes != 0)
        {
            const uint64_t used = m_Counter.UsedBytes();
            const uint64_t need = size + kHeaderSize;
            if (used + need > s_MaxBytes)
            {
                m_Counter.onOOM();
                if (s_OOMAction == OOMAction::Abort)
                {
                    std::fprintf(stderr,
                                 "[XMem] OOM: budget %llu exceeded "
                                 "(used=%llu, need=%llu)\n",
                                 (unsigned long long)s_MaxBytes,
                                 (unsigned long long)used,
                                 (unsigned long long)need);
                    std::terminate();
                }
                if (s_OOMAction == OOMAction::ReturnNull)
                    return nullptr;
                // Expand：继续分配（已记 OOM）
            }
        }

        // ── ① 多要一块 header 的空间 ──
        void *raw = backend->allocate(size + kHeaderSize);
        if (!raw)
        {
            m_Counter.onOOM();
            if (s_OOMAction == OOMAction::Abort)
                std::terminate();
            return nullptr;
        }

        // ── ② 在头部写清"我是谁"（释放时据此找对后端）──
        auto *hdr = static_cast<AllocHeader *>(raw);
        hdr->magic = kAllocMagic;
        hdr->backend = static_cast<uint8_t>(resolved);
        hdr->size = size;

        // ── ③ 返回 header 之后的用户区 ──
        void *user = static_cast<uint8_t *>(raw) + kHeaderSize;

        // ── ④ 登记到"已分配表"（供 deallocate 安全判定归属）──
        if (!m_OwnedTable.insert(user))
        {
            // 登记失败（内存不足）→ 退回，不能返回一个"不认识的"指针
            backend->deallocate(raw, size + kHeaderSize);
            m_Counter.onOOM();
            if (s_OOMAction == OOMAction::Abort)
                std::terminate();
            return nullptr;
        }

        m_Counter.onAllocate(size, resolved);
        m_Counter.onOverhead(kHeaderSize);
        return user;
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  释放
    // ═════════════════════════════════════════════════════════════════════════

    void Memory::deallocate(void *ptr, uint64_t size)
    {
        if (!ptr)
            return;

        // ── 先做安全探测：只有确认是门面发的，才去读分配头 ──
        // ⚠️ 不能直接读 ptr - kHeaderSize：外部指针那里可能是未映射内存，
        //    甚至正好压在页边界上 → 段错误。故先查 OwnedSet。
        if (!m_OwnedTable.contains(ptr))
        {
            // 不是门面发的（CRT 的、或开关关闭期间分配的）→ 交回标准库
            std::free(ptr);
            return;
        }

        void *raw = static_cast<uint8_t *>(ptr) - kHeaderSize;
        auto *hdr = static_cast<AllocHeader *>(raw);

        if (hdr->magic != kAllocMagic)
        {
            // 在册但头不对 —— 重复释放或指针被改过。报告并兜底。
            std::fprintf(stderr,
                         "[XMem] deallocate: corrupt header %p (double free?)\n",
                         ptr);
            m_OwnedTable.erase(ptr);
            std::free(raw);
            return;
        }

        const uint64_t realSize = hdr->size;
        if (size != 0 && size != realSize)
        {
            std::fprintf(stderr,
                         "[XMem] deallocate: size mismatch %p "
                         "(given=%llu, actual=%llu)\n",
                         ptr, (unsigned long long)size,
                         (unsigned long long)realSize);
        }

        const BackendType bt = static_cast<BackendType>(hdr->backend);
        IMemoryBackend *backend = backendFor(bt);

        hdr->magic = 0;          // 防重复释放时误判
        m_OwnedTable.erase(ptr); // 从"已分配表"摘除

        m_Counter.onDeallocate(realSize, bt);
        if (backend)
            backend->deallocate(raw, realSize + kHeaderSize);
        else
            std::free(raw); // 理论到不了；兜底不泄漏
    }

    // ═════════════════════════════════════════════════════════════════════════
    //  关闭
    // ═════════════════════════════════════════════════════════════════════════

    void Memory::Shutdown()
    {
        const uint64_t live = liveBlocks();
        if (live != 0)
        {
            std::fprintf(stderr,
                         "[XMem] Shutdown: %llu blocks still alive "
                         "(leak suspected; see Memory::stats())\n",
                         (unsigned long long)live);
        }
        m_Shutdown = true;
    }

} // namespace X_Y
