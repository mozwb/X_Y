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
}

namespace xy
{
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

    const XyHostV1* Host()
    {
        static const XyHostV1 kHost = {
            sizeof(XyHostV1),
            XY_ABI_VERSION,
            nullptr,
            &HostLog,
            &HostAlloc,
            &HostFree,
        };
        return &kHost;
    }
}
