#pragma once
/*
KernelCommands.h —— 内核内建命令的注册入口。

内建命令都挂在 EngineContext 上，owner 统一是 "engine"：
  help / commands / modules / load / unload / program / mem / abi

exec 还没进：它要 Platform/Process，等那层写好再补。
*/

namespace X_Y
{
    struct EngineContext;

    // 把内建命令登记进 ctx.commands；同一进程只调一次。
    void RegisterKernelCommands(EngineContext& ctx);
}
