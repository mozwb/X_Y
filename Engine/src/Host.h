#pragma once
/*
Host.h —— 内核运行期上下文，以及 XyHostV1 的构造入口。

EngineContext 是内核的全部共享状态：
  - services：模块之间通信的名字表
  - commands：命令字典
  - currentModule：当前归属者（"engine" 或模块名），host 的 add/remove
    靠它在注册时标注、在删除时校验
  - interactive：是否交互模式，Shell 用
  - host：传给 DLL 的 C 表

【生命周期】
  services / commands 必须比任何使用它们的对象活得久。
  典型 main：

      EngineContext ctx;
      const XyHostV1* host = BuildHost(ctx);
      ModuleRegistry modules(*host, ctx.services, ctx.commands, ctx.currentModule);
      ...

  ctx 先声明，modules 后声明；作用域退出时 modules 先析构，
  它摘命令/服务时 ctx 还活着。

【host 表】
  只放跨边界的东西：alloc / free / log / 三个服务回调。
  文件、模块装载等以后追加在尾部，不动老字段。
*/

#include "CommandTable.h"
#include "Services.h"

#include <xy_api.h>

#include <string>

namespace X_Y
{
    class ModuleRegistry;

    struct EngineContext
    {
        Services services;
        CommandTable commands;

        // 由 main 在构造后挂上；KernelCommands 的 load/unload/modules 用它。
        ModuleRegistry* modules = nullptr;

        // 正在初始化/执行的模块名；空 = 内核自己。
        // ModuleRegistry 在调用 DLL 入口前后设置，模块无法伪装。
        std::string currentModule;

        // 交互模式：Shell 决定要不要打提示符。
        bool interactive = false;

        // program 一次性执行期间为 true；此时 host->add_service 会被拒绝。
        bool oneShot = false;

        XyHostV1 host{};
    };

    // 填 ctx.host 并返回 &ctx.host。只调一次；之后指针不变。
    // 同时把 Memory 的 OOM 策略设成 ReturnNull：host->alloc 失败返回 NULL，
    // 不能因为模块要了一块大内存就把整个进程 terminate 掉。
    const XyHostV1* BuildHost(EngineContext& ctx);
}
