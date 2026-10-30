#pragma once
/*
xy_command.h —— 命令的形状，以及命令表入口。

xy_api.h 管“DLL 怎么被认出来、能从内核拿到什么”；
这个头管“DLL 能提供哪些命令”。

【谁用】
  - 内核：load 时调 xy_commands_v1(host)，把表里的命令登记进命令表。
  - DLL：定义静态 XyCommandV1 数组，导出 xy_commands_v1。

【硬规矩】
  1. 命令表、命令对象都是静态的，归 DLL 所有，必须活到卸载。
  2. 表以 NULL 结尾；没有命令就返回 NULL。
  3. 内核只读，不复制、不修改、不释放。
  4. argv[0] 是命令名，argv[argc] 保证为 NULL；argv 只在调用期间有效。
  5. name 非空、全局唯一、大小写敏感；建议全小写。
  6. invoke 返回值就是退出码，0 = 成功。
  7. self 由 DLL 定义，内核原样回传，不懂语义。
  8. 结构体只允许尾部追加；读新字段前用 size 判断。

【子命令】
  可以注册成 "pkg.list" / "pkg.install" 这种带点的名字，
  也可以只注册一个 "pkg"，在 invoke 里看 argv[1] 再分派。
  两种内核都支持，选一种就好。
*/

#include "xy_api.h"

/* 命令表入口符号名。 */
#define XY_COMMANDS_ENTRY "xy_commands_v1"

/*
 * 一条命令的调用约定。
 *   self：注册方私有指针，内核原样回传。
 *   argc/argv：同 main，argv[0] 是命令名，argv[argc] 保证为 NULL。
 *   返回：退出码，0 = 成功。
 */
typedef int (*XyCmdInvokeFn)(void* self, int argc, const char* const* argv);

typedef struct XyCommandV1
{
    uint32_t size;        /* = sizeof(XyCommandV1) */
    const char* name;     /* "hello"；非空、全局唯一 */
    const char* help;     /* 一行说明，可为 NULL */
    XyCmdInvokeFn invoke; /* 不可为空 */
    void* self;           /* DLL 私有，内核原样回传；不需要就 NULL */
} XyCommandV1;

#define XY_COMMAND_HAS(cmd, field) \
    ((cmd) != NULL && (cmd)->size >= (uint32_t)(offsetof(XyCommandV1, field) + sizeof((cmd)->field)))

/*
 * 命令表入口：DLL 可选导出。
 *   host：内核的 host 表；DLL 可以存下来给 invoke 用。
 *   返回：NULL 结尾的静态数组；没有命令返回 NULL。
 *   内核在 xy_abi_version_v1 检查通过后才调用。
 *   版本不符返回 NULL（内核按“没有命令”处理并报错）。
 */
typedef const XyCommandV1* const* (*xy_commands_v1_fn)(const XyHostV1* host);

/*
用法：

    static int Hello(void* self, int argc, const char* const* argv)
    {
        (void)self; (void)argc; (void)argv;
        return 0;
    }

    static const XyCommandV1 kHello = {
        sizeof(XyCommandV1), "hello", "打印 hello", &Hello, NULL
    };
    static const XyCommandV1* const kCommands[] = { &kHello, NULL };

    XY_EXPORT const XyCommandV1* const* xy_commands_v1(const XyHostV1* host)
    {
        if (!host || host->abi_version != XY_ABI_VERSION)
            return NULL;
        return kCommands;
    }
*/
