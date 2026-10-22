// Thief —— 反面夹具：试图删除别的模块注册的服务。
// 用来验证引擎会拒绝跨模块删除，而不是默默把别人的服务抹掉。

#include <xy_api.h>

XY_EXPORT int xy_module_v1(const XyHostV1* host)
{
    if (!host || !XY_HOST_HAS(host, remove_service))
    {
        return 1;
    }

    // probe.marker 属于 Probe；这里应该被引擎拒绝
    host->remove_service(host->user, "probe.marker");
    host->log(host->user, XY_LOG_WARN, "thief: 我试过删除 probe.marker");
    return 0;
}
