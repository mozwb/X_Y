#include "xy_api.h"
#include "xy_command.h"
#include "echo_api.h"

#include <stdio.h>

static const XyHostV1* g_host = NULL;

static int CmdEcho(void* self, int argc, const char* const* argv)
{
    (void)self;

    if (g_host == NULL)
    {
        return 3;
    }

    const EchoService* service =
        (const EchoService*)g_host->get_service(g_host->user, "echo");
    if (service == NULL)
    {
        fprintf(stderr, "echo service not found\n");
        return 3;
    }

    const char* text = argc >= 2 ? argv[1] : "";
    printf("%s\n", service->say(text));
    return 0;
}

static const XyCommandV1 kEcho = {
    sizeof(XyCommandV1), "echo", "调用 echo 服务", &CmdEcho, NULL
};

static const XyCommandV1* const kCommands[] = {&kEcho, NULL};

XY_EXPORT uint32_t xy_abi_version_v1(void)
{
    return XY_ABI_VERSION;
}

XY_EXPORT const XyCommandV1* const* xy_commands_v1(const XyHostV1* host)
{
    if (host == NULL || host->abi_version != XY_ABI_VERSION)
    {
        return NULL;
    }
    g_host = host;
    return kCommands;
}
