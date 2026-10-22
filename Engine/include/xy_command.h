#pragma once
/*
xy_command.h —— 一条命令的形状。

内核只认一个动词：run <命令名> [参数...]
命令本身（内核命令、dll 命令）都长这个样子，用同一个调用约定。

约定：
  1. 参数一律是字符串数组（脚本、shell、dll 命令共用一份解析）。
  2. argv[0] 是命令名，argv[argc] 保证为 NULL。
  3. 返回值是退出码，0 = 成功。输出走 host->log 或由调用方接管。
  4. 命令对象由**注册方**持有，生命周期不得短于它的反注册。
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 命令在服务表里的前缀：cmd.<name> */
#define XY_CMD_SERVICE_PREFIX "cmd."

typedef int (*XyCmdInvokeFn)(void* self, int argc, const char* const* argv);

typedef struct XyCommandV1
{
    unsigned size;         /* = sizeof(XyCommandV1) */
    const char* name;      /* "alloc" */
    const char* help;      /* 一行说明 */
    XyCmdInvokeFn invoke;  /* 不可为空 */
    void* self;            /* 注册方私有，原样回传给 invoke；不需要就 NULL */
} XyCommandV1;

#ifdef __cplusplus
}
#endif
