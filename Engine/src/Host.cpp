#include "Host.h"

#include <Memory/Memory.h>

#include <cstddef>
#include <cstdio>
#include <string>

namespace X_Y
{
    namespace
    {
        // Memory 只保证 max_align；更大的我们暂时不支持，直接返回 NULL，
        // 不在 host 里再混一套 aligned 分配。
        constexpr std::size_t kMaxAlign = alignof(std::max_align_t);

        const char* LevelTag(int level)
        {
            switch (level)
            {
            case XY_LOG_TRACE: return "trace";
            case XY_LOG_INFO:  return "info";
            case XY_LOG_WARN:  return "warn";
            case XY_LOG_ERROR: return "error";
            default:           return "?";
            }
        }

        void HostLog(void*, int level, const char* message)
        {
            std::fprintf(stderr, "[%s] %s\n",
                         LevelTag(level),
                         message != nullptr ? message : "(null)");
            std::fflush(stderr);
        }

        void* HostAlloc(void*, std::size_t size, std::size_t align)
        {
            if (size == 0)
            {
                return nullptr;
            }

            if (align != 0)
            {
                const bool powerOfTwo = (align & (align - 1)) == 0;
                if (!powerOfTwo || align > kMaxAlign)
                {
                    return nullptr;
                }
            }

            return Memory::Instance().allocate(size);
        }

        void HostFree(void*, void* ptr)
        {
            if (ptr != nullptr)
            {
                Memory::Instance().deallocate(ptr);
            }
        }

        int HostAddService(void* user, const char* name, void* ptr)
        {
            auto* ctx = static_cast<EngineContext*>(user);
            if (ctx == nullptr || name == nullptr || ptr == nullptr)
            {
                return -1;
            }

            // 一次性程序返回后 DLL 就卸了，它发布的指针活不过调用；
            // 在源头拒绝，比事后摘表干净。
            if (ctx->oneShot)
            {
                std::fprintf(stderr,
                             "X_Y: 一次性程序 %s 不能注册服务 '%s'\n",
                             ctx->currentModule.c_str(), name);
                return -1;
            }

            // 归属由引擎标注，模块无法伪装。
            const std::string owner =
                ctx->currentModule.empty() ? std::string("engine") : ctx->currentModule;
            return ctx->services.Add(name, ptr, owner) ? 0 : 1;
        }

        void* HostGetService(void* user, const char* name)
        {
            auto* ctx = static_cast<EngineContext*>(user);
            return (ctx != nullptr && name != nullptr) ? ctx->services.Get(name) : nullptr;
        }

        void HostRemoveService(void* user, const char* name)
        {
            auto* ctx = static_cast<EngineContext*>(user);
            if (ctx == nullptr || name == nullptr)
            {
                return;
            }

            // 注册时引擎标了归属，删除时必须验；currentModule 为空 = 引擎自己在删。
            if (!ctx->currentModule.empty())
            {
                const std::string owner = ctx->services.OwnerOf(name);
                if (!owner.empty() && owner != ctx->currentModule)
                {
                    std::fprintf(stderr,
                                 "X_Y: 拒绝 %s 删除他人服务 '%s'（归属 %s）\n",
                                 ctx->currentModule.c_str(), name, owner.c_str());
                    return;
                }
            }

            ctx->services.Remove(name);
        }
    }

    const XyHostV1* BuildHost(EngineContext& ctx)
    {
        // host->alloc 的契约是失败返回 NULL，不能因为一次大分配 terminate 进程。
        Memory::Instance().setOOMAction(OOMAction::ReturnNull);

        ctx.host.size = sizeof(XyHostV1);
        ctx.host.abi_version = XY_ABI_VERSION;
        ctx.host.user = &ctx;

        ctx.host.alloc = &HostAlloc;
        ctx.host.free = &HostFree;
        ctx.host.log = &HostLog;

        ctx.host.add_service = &HostAddService;
        ctx.host.get_service = &HostGetService;
        ctx.host.remove_service = &HostRemoveService;

        return &ctx.host;
    }
}
