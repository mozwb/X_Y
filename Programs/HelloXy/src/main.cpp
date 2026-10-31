// HelloXy —— 最小微程序：只认 xy_api.h，不链任何内核符号。
//
// 验证两条路：
//   1. X_Y --program HelloXy.dll ...
//   2. X_Y --module HelloXy.dll --run HelloXy ...
//
// 故意只导出 program 入口，让 load 那条路走“微程序包成命令”。

#include <xy_api.h>

#include <cstdio>

XY_EXPORT uint32_t xy_abi_version_v1(void)
{
    return XY_ABI_VERSION;
}

XY_EXPORT int xy_program_v1(const XyHostV1* host, int argc, const char* const* argv)
{
    if (host == nullptr || host->abi_version != XY_ABI_VERSION)
    {
        return 1;
    }

    host->log(host->user, XY_LOG_INFO, "HelloXy: start");

    std::printf("HelloXy\n");
    std::printf("  abi   = %u\n", static_cast<unsigned>(XY_ABI_VERSION));
    std::printf("  host  = %llu bytes\n",
                static_cast<unsigned long long>(host->size));
    std::printf("  argc  = %d\n", argc);

    for (int i = 0; i < argc; ++i)
    {
        std::printf("  argv[%d] = %s\n", i, argv[i] != nullptr ? argv[i] : "(null)");
    }

    void* block = host->alloc(host->user, 64, 0);
    if (block == nullptr)
    {
        host->log(host->user, XY_LOG_ERROR, "HelloXy: alloc 失败");
        return 2;
    }

    std::printf("  alloc = %p\n", block);
    host->free(host->user, block);

    host->log(host->user, XY_LOG_INFO, "HelloXy: done");
    return 0;
}
