#pragma once
/*
xy_api.h —— X_Y 内核与 DLL 之间唯一的 ABI 底座。

这个头只回答三件事：
  1. DLL 怎么被内核认出来（入口名、版本）
  2. DLL 能从内核拿到什么（host 表）
  3. 跨边界传参的规矩

它不包含：
  - 命令的形状            -> xy_command.h
  - 内核实现              -> Host / ModuleRegistry / CommandTable
  - 任何 C++ 类型、STL、平台头

【使用时刻】
  载入：内核调 xy_abi_version_v1()，版本不等直接拒载。
  装载：内核把 XyHostV1 交给 xy_commands_v1(host)，换回命令表。
  运行：内核调 xy_program_v1(host, argc, argv)，argv[0] 是程序名。
  卸载：内核摘命令、摘服务、FreeLibrary；v1 没有 DLL 清理回调。

【硬规矩】
  1. 只传 C 类型。不传 std::string / 容器 / 异常 / 引用。
  2. 内存成对：host->alloc 发的只能 host->free；DLL new 的自己 delete。
  3. 结构体只允许尾部追加字段；读之前先用 size 判断。
  4. abi_version 不等 = 拒载，不做兼容。
  5. 字符串（argv、服务名、log 文本）只在调用期间有效，DLL 不得存指针。
  6. host 指针进程内唯一且一直有效，DLL 不得释放它。
*/

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#  define XY_EXTERN_C extern "C"
#else
#  define XY_EXTERN_C
#endif

#if defined(_WIN32)
#  define XY_EXPORT_SYMBOL __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#  define XY_EXPORT_SYMBOL __attribute__((visibility("default")))
#else
#  define XY_EXPORT_SYMBOL
#endif

/* 导出入口时用：XY_EXPORT uint32_t xy_abi_version_v1(void) { ... } */
#define XY_EXPORT XY_EXTERN_C XY_EXPORT_SYMBOL

/* 任何不向后兼容的改动都必须 +1。 */
#define XY_ABI_VERSION 1u

/* 入口符号名。文件名只负责定位，符号名负责演进。 */
#define XY_ABI_VERSION_ENTRY "xy_abi_version_v1"
#define XY_PROGRAM_ENTRY     "xy_program_v1"
#define XY_MODULE_ENTRY      "xy_module_v1"
#define XY_MODULE_SHUTDOWN_ENTRY "xy_module_shutdown_v1"
/* 命令表入口在 xy_command.h：XY_COMMANDS_ENTRY */

/* 日志级别。 */
enum XyLogLevel
{
    XY_LOG_TRACE = 0,
    XY_LOG_INFO  = 1,
    XY_LOG_WARN  = 2,
    XY_LOG_ERROR = 3
};

/*
 * host 服务表。
 *
 * 内核启动时填一次，之后指针不变；DLL 只持 const 指针。
 * 字段只增不改：新能力追加到末尾，并把 size 算对。
 *
 * alloc:
 *   align = 0 表示默认对齐。v1 只支持 <= 平台 max_align 的对齐；
 *   超出返回 NULL（Memory 目前不支持 over-align）。
 *   发出的指针只能交给 free。
 *
 * free:
 *   只接受 alloc 发出的指针。
 *
 * log:
 *   message 是 UTF-8 C 字符串，内核立刻打印/复制，不保留指针。
 *
 * add_service / get_service / remove_service:
 *   模块之间通信的唯一机制。内核只存 名字 -> void*，不懂语义。
 *   add 成功返回 0，重名返回非 0。
 *   get 找不到返回 NULL。
 *   remove 只能删自己注册的；名字不存在忽略。
 *   服务指针的生命周期由注册方保证，至少到 remove / 卸载。
 */
typedef void* (*XyAllocFn)(void* user, size_t size, size_t align);
typedef void  (*XyFreeFn)(void* user, void* ptr);
typedef void  (*XyLogFn)(void* user, int level, const char* message);

typedef int   (*XyAddServiceFn)(void* user, const char* name, void* ptr);
typedef void* (*XyGetServiceFn)(void* user, const char* name);
typedef void  (*XyRemoveServiceFn)(void* user, const char* name);

typedef struct XyHostV1
{
    uint32_t size;        /* = sizeof(XyHostV1)，使用方据此判断字段是否存在 */
    uint32_t abi_version; /* 必须等于 XY_ABI_VERSION */

    void* user; /* 内核私有，原样回传给下面所有回调 */

    XyAllocFn alloc;
    XyFreeFn  free;
    XyLogFn   log;

    XyAddServiceFn    add_service;
    XyGetServiceFn    get_service;
    XyRemoveServiceFn remove_service;
} XyHostV1;

#define XY_HOST_HAS(host, field) \
    ((host) != NULL && (host)->size >= (uint32_t)(offsetof(XyHostV1, field) + sizeof((host)->field)))

/*
 * 版本入口：DLL 必须导出。
 * 内核在调用任何别的入口之前先调它；版本不等直接拒载。
 * xy doctor 也可以不执行 DLL 就查它的 ABI。
 */
typedef uint32_t (*xy_abi_version_v1_fn)(void);

/*
 * 模块入口：DLL 可选导出。
 *   在版本检查之后、xy_commands_v1 之前调用，用来注册服务。
 *   返回 0 表示成功；非 0 则整次装载回滚。
 *   不要在这里注册命令；命令走 xy_commands_v1。
 *   host 在进程内一直有效，可以存下来给 shutdown 用。
 */
typedef int (*xy_module_v1_fn)(const XyHostV1* host);

/*
 * 模块卸载入口：DLL 可选导出。
 *   在摘命令/服务之前调用；provider 必须在这里把 xy_module_v1 注册的服务
 *   全部 remove 掉。内核随后还会按 owner 反查一次，漏掉的算泄漏。
 */
typedef void (*xy_module_shutdown_v1_fn)(void);

/*
 * 微程序入口：DLL 可选导出。
 *   argv[0] 是程序名（DLL 文件名，不含路径），argv[argc] 保证为 NULL。
 *   返回值就是退出码，0 = 成功。
 *   host 在调用期间及之后都有效；argv 只在调用期间有效。
 */
typedef int (*xy_program_v1_fn)(const XyHostV1* host, int argc, const char* const* argv);
