#include "Host.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#  include <malloc.h>
#endif

namespace
{
    // 只走一条分配路径：这样 HostFree 不需要知道调用方传了什么 align。
    constexpr size_t kDefaultAlign = alignof(std::max_align_t);

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

    using xy::EngineContext;

    void HostLog(void*, int level, const char* message)
    {
        std::fprintf(stderr, "[%s] %s\n", LevelTag(level), message ? message : "(null)");
        std::fflush(stderr);
    }

    // 成对约定的下半段：这里发出的指针只能由 HostFree 归还。
    void* HostAlloc(void*, size_t size, size_t align)
    {
        if (size == 0)
        {
            return nullptr;
        }
        const size_t a = align ? align : kDefaultAlign;
#if defined(_WIN32)
        return _aligned_malloc(size, a);
#else
        void* p = nullptr;
        return posix_memalign(&p, a, size) == 0 ? p : nullptr;
#endif
    }

    void HostFree(void*, void* ptr)
    {
        if (!ptr)
        {
            return;
        }
#if defined(_WIN32)
        _aligned_free(ptr);
#else
        std::free(ptr); // posix_memalign 的指针可以用 free
#endif
    }

    int HostAddService(void* user, const char* name, void* ptr)
    {
        auto* ctx = static_cast<EngineContext*>(user);
        if (!ctx || !name)
        {
            return -1;
        }
        // 归属由引擎判定，模块无法伪装
        const std::string owner = ctx->currentModule.empty() ? "engine" : ctx->currentModule;
        return ctx->services.Add(name, ptr, owner);
    }

    void* HostGetService(void* user, const char* name)
    {
        auto* ctx = static_cast<EngineContext*>(user);
        return (ctx && name) ? ctx->services.Get(name) : nullptr;
    }

    void HostRemoveService(void* user, const char* name)
    {
        auto* ctx = static_cast<EngineContext*>(user);
        if (ctx && name)
        {
            ctx->services.Remove(name);
        }
    }
}

namespace xy
{
    const XyHostV1* BuildHost(EngineContext& ctx)
    {
        ctx.host.size = sizeof(XyHostV1);
        ctx.host.abi_version = XY_ABI_VERSION;
        ctx.host.user = &ctx;
        ctx.host.log = &HostLog;
        ctx.host.alloc = &HostAlloc;
        ctx.host.free = &HostFree;
        ctx.host.add_service = &HostAddService;
        ctx.host.get_service = &HostGetService;
        ctx.host.remove_service = &HostRemoveService;
        return &ctx.host;
    }
}
