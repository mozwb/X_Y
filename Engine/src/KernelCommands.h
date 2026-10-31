#pragma once
/*
KernelCommands.h —— 内核内建命令的注册入口。

内建命令都挂在 Engine 上，owner 统一是 "engine"：
  help / commands / modules / engine / load / unload / program / exec / mem / abi
*/

namespace X_Y
{
    struct Engine;

    // 把内建命令登记进 engine.commands；同一进程只调一次。
    void RegisterKernelCommands(Engine& engine);
}
