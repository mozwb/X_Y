#pragma once
/*
xy_kernel.h —— 内核命令分发服务（C ABI）。

微程序 / 插件拿不到 Engine.commands，只能通过这个服务把命令交回内核：

    const XyKernelV1* kernel =
        (const XyKernelV1*)host->get_service(host->user, XY_KERNEL_SERVICE);

    const char* argv[] = { "help" };
    kernel->run(kernel->user, 1, argv);

run 的查找顺序和 Shell 一致：
    Engine.commands -> container.shared commands -> root.commands

argv[0] 是命令名，argv 不需要以 NULL 结尾；argc 是有效参数个数。
返回值是命令退出码；找不到命令返回 127。
*/

#include "xy_api.h"

#include <stdint.h>

#define XY_KERNEL_SERVICE "kernel"
#define XY_KERNEL_VERSION 1u

typedef struct XyKernelV1
{
    unsigned size;
    unsigned version;

    // 指向注册这个服务的 Engine；插件原样传回。
    void* user;

    int (*run)(void* user, int argc, const char* const* argv);
} XyKernelV1;
