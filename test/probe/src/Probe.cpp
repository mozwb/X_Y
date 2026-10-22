// Probe —— B2 的测试模块：只验证「装载 → 注册 → 反注册 → 卸载」这条链路。
// 它不依赖任何 X_Y 模块，所以能单独证明注册表机制本身是对的。

#include <xy_api.h>

namespace
{
    const XyHostV1* g_host = nullptr;

    // 故意留在 .bss 里，卸载后如果还有人拿到它，就是悬垂
    int g_marker = 0x5859;
}

XY_EXPORT int xy_module_v1(const XyHostV1* host)
{
    if (!host || host->abi_version != XY_ABI_VERSION)
    {
        return 1;
    }
    // 追加段要按 size 判断，老引擎不会给这几个字段
    if (!XY_HOST_HAS(host, add_service))
    {
        host->log(host->user, XY_LOG_ERROR, "probe: host 没有服务表，跳过注册");
        return 2;
    }

    g_host = host;
    if (host->add_service(host->user, "probe.marker", &g_marker) != 0)
    {
        host->log(host->user, XY_LOG_ERROR, "probe: 注册 probe.marker 失败");
        return 3;
    }

    host->log(host->user, XY_LOG_INFO, "probe: 已装载并注册 probe.marker");
    return 0;
}

XY_EXPORT void xy_module_shutdown_v1(void)
{
    if (!g_host)
    {
        return;
    }
    g_host->remove_service(g_host->user, "probe.marker");
    g_host->log(g_host->user, XY_LOG_INFO, "probe: 已反注册，准备卸载");
    g_host = nullptr;
}
