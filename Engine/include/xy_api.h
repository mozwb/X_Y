#pragma once
/*
xy_api.h —— 引擎与 程序/模块 之间唯一的 ABI 契约。

规则（违反即 UB，不救）：
  1. 跨边界只传 C 类型：POD、函数指针、C 字符串、裸字节。不传 std::string / 容器 / 异常。
  2. 内存成对：host->alloc 发的只能 host->free 还；谁 new 的谁 delete。别跨堆。
  3. 结构体只允许在**尾部追加**字段。使用方先看 size，够长才读。
  4. abi_version 不等 = 拒绝加载，不做兼容。

本文件同时被 C 和 C++ 包含，不得出现 C++ 语法。
*/

#include <stddef.h>

#ifdef __cplusplus
#  define XY_EXTERN_C extern "C"
#else
#  define XY_EXTERN_C
#endif

#if defined(_WIN32)
#  define XY_EXPORT_SYMBOL __declspec(dllexport)
#  define XY_IMPORT_SYMBOL __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#  define XY_EXPORT_SYMBOL __attribute__((visibility("default")))
#  define XY_IMPORT_SYMBOL
#else
#  define XY_EXPORT_SYMBOL
#  define XY_IMPORT_SYMBOL
#endif

/* 模块/程序导出入口时用 */
#define XY_EXPORT XY_EXTERN_C XY_EXPORT_SYMBOL

/* 引擎 ABI 版本。任何一次不向后兼容的改动都必须 +1。 */
#define XY_ABI_VERSION 1u

/* 入口符号名 —— 文件名只负责定位，符号名负责演进 */
#define XY_PROGRAM_ENTRY "xy_program_v1"
#define XY_MODULE_ENTRY "xy_module_v1"

/* 可选：模块卸载前必须调用，用来反注册自己往 host 里塞的一切 */
#define XY_MODULE_SHUTDOWN_ENTRY "xy_module_shutdown_v1"

/* 日志级别 */
enum XyLogLevel
{
    XY_LOG_TRACE = 0,
    XY_LOG_INFO = 1,
    XY_LOG_WARN = 2,
    XY_LOG_ERROR = 3
};

typedef void (*XyLogFn)(void* user, int level, const char* message);
typedef void* (*XyAllocFn)(void* user, size_t size, size_t align);
typedef void (*XyFreeFn)(void* user, void* ptr);

/* 服务表：模块之间通信的唯一机制。内核只存名字→指针，不懂语义。 */
typedef int (*XyAddServiceFn)(void* user, const char* name, void* ptr);
typedef void* (*XyGetServiceFn)(void* user, const char* name);
typedef void (*XyRemoveServiceFn)(void* user, const char* name);

/*
 * host 服务表。引擎拥有实例，程序/模块只持 const 指针。
 * 只增不改：新能力一律追加在末尾，并把 size 算对。
 */
typedef struct XyHostV1
{
    unsigned size;        /* = sizeof(XyHostV1)，使用方据此判断字段是否存在 */
    unsigned abi_version; /* 必须等于 XY_ABI_VERSION */

    void* user; /* 引擎私有，回传给下面所有回调 */

    XyLogFn log;    /* 不可为空 */
    XyAllocFn alloc;/* 不可为空；align 为 0 表示默认对齐 */
    XyFreeFn free;  /* 不可为空；只接受 alloc 发出的指针 */

    /* v1 追加段 */
    XyAddServiceFn add_service;      /* 名字重复返回非 0 */
    XyGetServiceFn get_service;      /* 找不到返回 NULL */
    XyRemoveServiceFn remove_service;/* 名字不存在忽略 */
} XyHostV1;

/* 使用方在读取上面追加段之前，用这个判断字段是否存在 */
#define XY_HOST_HAS(host, field) \
    ((host) && (host)->size >= (unsigned)(offsetof(XyHostV1, field) + sizeof((host)->field)))

/*
 * 程序入口：引擎 LoadLibrary 后按 XY_PROGRAM_ENTRY 取符号并调用。
 * 返回值即进程退出码。
 * argv[0] 是程序名（不是 dll 路径），argv[argc] 保证为 NULL。
 */
typedef int (*xy_program_v1_fn)(const XyHostV1* host, int argc, const char* const* argv);

/*
 * 模块入口：引擎装载模块后调用，用于注册服务/命令。
 * 返回 0 表示成功。
 */
typedef int (*xy_module_v1_fn)(const XyHostV1* host);

/*
 * 模块卸载入口（可选）：引擎在 FreeLibrary **之前**调用。
 * 模块必须在这里把 xy_module_v1 里注册的东西全部反注册，否则 engine 侧会留悬垂指针。
 * 引擎会检查：卸载后若仍能查到该模块注册的服务，判定为泄漏并报警。
 */
typedef void (*xy_module_shutdown_v1_fn)(void);
