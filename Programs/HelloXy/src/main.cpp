// HelloXy —— 最小可运行程序，用来验证引擎边界。
// 它只认 xy_api.h，不链引擎、不链任何模块：先证明「载入 → 调用 → 归还」通。

#include <xy_api.h>

#include <cstring>

namespace
{
    void Say(const XyHostV1* host, int level, const char* text)
    {
        host->log(host->user, level, text);
    }
}

XY_EXPORT int xy_program_v1(const XyHostV1* host, int argc, const char* const* argv)
{
    if (!host)
    {
        return 70;
    }

    // 结构体版本化：先看 ABI 号，再看 size 够不够我们已知的字段
    if (host->abi_version != XY_ABI_VERSION)
    {
        return 71;
    }
    if (host->size < sizeof(XyHostV1))
    {
        return 72;
    }

    Say(host, XY_LOG_INFO, "HelloXy: 引擎已交接 host 表");

    for (int i = 0; i < argc; ++i)
    {
        Say(host, XY_LOG_TRACE, argv[i] ? argv[i] : "(null)");
    }

    // 走一遍 host 的分配器，验证成对约定（必须用 host->free 归还）
    void* block = host->alloc(host->user, 128, 0);
    if (!block)
    {
        Say(host, XY_LOG_ERROR, "HelloXy: host->alloc 返回空");
        return 73;
    }
    std::memset(block, 0, 128);
    host->free(host->user, block);

    Say(host, XY_LOG_INFO, "HelloXy: 结束");
    return 0;
}
