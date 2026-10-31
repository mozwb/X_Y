#include "xy_api.h"
#include "echo_api.h"

#include <stddef.h>

static const XyHostV1* g_host = NULL;

static const char* EchoSay(const char* text)
{
    return text != NULL ? text : "";
}

static const EchoService g_service = {&EchoSay};

XY_EXPORT uint32_t xy_abi_version_v1(void)
{
    return XY_ABI_VERSION;
}

XY_EXPORT int xy_module_v1(const XyHostV1* host)
{
    if (host == NULL || host->abi_version != XY_ABI_VERSION)
    {
        return 1;
    }
    g_host = host;
    return host->add_service(host->user, "echo", (void*)&g_service);
}

XY_EXPORT void xy_module_shutdown_v1(void)
{
    if (g_host != NULL)
    {
        g_host->remove_service(g_host->user, "echo");
        g_host = NULL;
    }
}
