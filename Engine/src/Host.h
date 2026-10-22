// 引擎运行期上下文 + host 表实现。
// host->user 指向 EngineContext，所以所有回调都能拿到引擎状态。
#pragma once

#include <xy_api.h>

#include "Services.h"

#include <map>
#include <string>

namespace X_Y
{
    struct EngineContext
    {
        Services services;

        // 内核命令 alloc 发出的块：地址 → 字节数。free 只认这里登记过的地址。
        std::map<void*, size_t> blocks;

        // 正在初始化的模块名。为空表示「内核自己在注册」。
        // 由 ModuleRegistry 在调用 xy_module_v1 前后设置，
        // 这样模块不需要自己报名字，也说不了谎。
        std::string currentModule;

        // 交互模式：命令输出走 stdout；否则也可以再改
        bool interactive = false;

        XyHostV1 host{};
    };

    // 填好 ctx.host 并返回指向它的指针（生命周期由 ctx 决定）
    const XyHostV1* BuildHost(EngineContext& ctx);
}
