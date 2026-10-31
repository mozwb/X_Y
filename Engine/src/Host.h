#pragma once
/*
Host.h —— 一个 Engine 实例，以及 XyHostV1 的构造入口。

Engine 是内核的一组运行期状态；每个 Engine 一份：
  - services：这个 engine 的服务表
  - commands：这个 engine 的命令字典
  - currentModule：当前归属者（"engine" 或模块名）
  - interactive / oneShot
  - host：传给 DLL 的 C 表，host.user 指向这个 Engine

以后 EngineContainer 可以创建多个 Engine；现在 main 只创建根 Engine，
所以行为不变。

【生命周期】
  services / commands 必须比任何使用它们的对象活得久。
  典型 main：

      Engine engine;
      const XyHostV1* host = BuildHost(engine);
      ModuleRegistry modules(*host, engine.services, engine.commands, engine.currentModule);
      ...

  engine 先声明，modules 后声明；作用域退出时 modules 先析构，
  它摘命令/服务时 engine 还活着。

【host 表】
  只放跨边界的东西：alloc / free / log / 三个服务回调。
  文件、模块装载等以后追加在尾部，不动老字段。
*/

#include "CommandTable.h"
#include "ModuleRegistry.h"
#include "Services.h"

#include <xy_api.h>

#include <memory>
#include <string>

namespace X_Y
{
    class EngineContainer;

    struct Engine
    {
        std::string name = "root";
        EngineContainer* container = nullptr;

        Services services;
        CommandTable commands;

        // 这个 engine 自己的模块表；Start() 里建，析构时先于 services/commands。
        std::unique_ptr<ModuleRegistry> modules;

        // 正在初始化/执行的模块名；空 = 内核自己。
        // ModuleRegistry 在调用 DLL 入口前后设置，模块无法伪装。
        std::string currentModule;

        // 交互模式：Shell 决定要不要打提示符。
        bool interactive = false;

        // program 一次性执行期间为 true；此时 host->add_service 会被拒绝。
        bool oneShot = false;

        XyHostV1 host{};

        // 填 host、markBaseline、建 modules；withBuiltins=true 时登记内建命令。
        // 根 engine 用 true；容器里的子 engine 用 false，避免每个子 engine
        // 都带一套 engine/load/unload 命令。
        void Start(bool withBuiltins = true);
    };

    // 1.x 兼容名；新代码用 Engine。
    using EngineContext = Engine;

    // 填 engine.host 并返回 &engine.host。只调一次；之后指针不变。
    // 同时把 Memory 的 OOM 策略设成 ReturnNull：host->alloc 失败返回 NULL，
    // 不能因为模块要了一块大内存就把整个进程 terminate 掉。
    const XyHostV1* BuildHost(Engine& engine);
}
