#pragma once

namespace X_Y {

// 平台消息循环——纯虚接口
class PlatformLoop {
public:
    virtual ~PlatformLoop() = default;

    // 平台启动引导：各平台自行实现初始化（DPI、窗口类注册、控制台编码等）
    // Application 在启动时调用一次，不掺平台 #ifdef
    virtual void Boot() = 0;

    // 泵取并分发一条消息。返回 false 表示收到退出消息。
    virtual bool PumpMessage() = 0;
};

// 平台工厂
class PlatformLoopFactory {
public:
    static PlatformLoop* Create();
};

} 
// namespace X_Y
