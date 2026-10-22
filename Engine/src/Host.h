// 日志与内存：引擎侧实现，通过 host 表交给程序/模块使用。
#pragma once

#include <xy_api.h>

namespace xy
{
    // 安装到 host->log / alloc / free 的实现
    void HostLog(void* user, int level, const char* message);
    void* HostAlloc(void* user, size_t size, size_t align);
    void HostFree(void* user, void* ptr);

    // 当前进程可用的 host 表（引擎自用，与交给程序的同一份）
    const XyHostV1* Host();
}
