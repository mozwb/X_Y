// 内核命令：alloc / free / mem / help / commands
// 它们和 dll 命令走同一个 XyCommandV1 形状、同一张命令表，没有特权分支。
#pragma once

namespace X_Y
{
    struct EngineContext;

    // 把内核命令全部登记进 ctx.services（前缀 cmd.）
    void RegisterKernelCommands(EngineContext& ctx);
}
