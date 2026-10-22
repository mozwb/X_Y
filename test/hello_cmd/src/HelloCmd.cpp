// HelloCmd —— 一个 dll 提供命令的最小例子。
// 它证明：dll 只要能拿到 host，就能往命令表里挂命令，内核不用为它写任何代码。

#include <xy_api.h>
#include <xy_command.h>

#include <cstdio>
#include <cstring>

namespace
{
    const XyHostV1* g_host = nullptr;

    int HelloInvoke(void* self, int argc, const char* const* argv)
    {
        auto* host = static_cast<const XyHostV1*>(self);
        char line[512];
        int used = std::snprintf(line, sizeof(line), "hello:");
        for (int i = 1; i < argc && used > 0 && used < static_cast<int>(sizeof(line)); ++i)
        {
            used += std::snprintf(line + used, sizeof(line) - static_cast<size_t>(used),
                                  " %s", argv[i]);
        }
        host->log(host->user, XY_LOG_INFO, line);
        return 0;
    }

    int FailInvoke(void*, int, const char* const*)
    {
        return 42; // 用来验证退出码能透传
    }

    XyCommandV1 kHello = {sizeof(XyCommandV1), "hello", "打招呼：hello [名字...]", &HelloInvoke, nullptr};
    XyCommandV1 kFail = {sizeof(XyCommandV1), "fail", "总是返回 42", &FailInvoke, nullptr};
}

XY_EXPORT int xy_module_v1(const XyHostV1* host)
{
    if (!host || host->abi_version != XY_ABI_VERSION)
    {
        return 1;
    }
    if (!XY_HOST_HAS(host, add_service))
    {
        return 2;
    }

    g_host = host;
    kHello.self = const_cast<XyHostV1*>(host); // 命令自己拿不到 ctx，就把 host 传回去
    kFail.self = nullptr;

    if (host->add_service(host->user, XY_CMD_SERVICE_PREFIX "hello", &kHello) != 0)
    {
        return 3;
    }
    if (host->add_service(host->user, XY_CMD_SERVICE_PREFIX "fail", &kFail) != 0)
    {
        host->remove_service(host->user, XY_CMD_SERVICE_PREFIX "hello");
        return 3;
    }

    host->log(host->user, XY_LOG_INFO, "hello_cmd: 注册了 hello / fail");
    return 0;
}

XY_EXPORT void xy_module_shutdown_v1(void)
{
    if (!g_host)
    {
        return;
    }
    g_host->remove_service(g_host->user, XY_CMD_SERVICE_PREFIX "hello");
    g_host->remove_service(g_host->user, XY_CMD_SERVICE_PREFIX "fail");
    g_host = nullptr;
}
