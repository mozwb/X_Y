// Leaky —— 反面夹具：注册服务却不导出 shutdown，用来验证引擎能自己查出泄漏。
// 这个模块的存在意义就是「坏」，不要照它写。

#include <xy_api.h>

namespace
{
    int g_marker = 0;
}

XY_EXPORT int xy_module_v1(const XyHostV1* host)
{
    if (!host || !XY_HOST_HAS(host, add_service))
    {
        return 1;
    }
    host->add_service(host->user, "leaky.marker", &g_marker);
    host->log(host->user, XY_LOG_WARN, "leaky: 我注册了服务，但我不打算反注册");
    return 0;
}
