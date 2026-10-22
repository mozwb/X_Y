# X_Y DEVLOG

> 最新在上。设计文档见 `_notes/REFACTOR_PLAN.md`。

---

## 2026-10-22（四）· B2 模块装载 + 内建/DLL 分界

### 定位（按你的划分落地）

| 类别 | 模块 | 产物 | 说明 |
| --- | --- | --- | --- |
| **引擎内建** | Core / Memory / Timer / Log / Files | `lib*.a` 或 header | 写死 `STATIC`，不跟随 `-DXY_MODULE_LINKAGE` |
| **工具侧（将来 dll）** | Math / DataStore / Decoder / Input | 现在仍是 `lib` | 切 `SHARED` 前必须先解决「内建符号从哪来」，见下 |
| **可运行程序** | `Programs/*` | `*.dll` + `xy_program_v1` | |
| **测试夹具** | `test/probe`、`test/leaky` | `*.dll` | 只验证装载机制，不是产品 |

vendor 也解耦了：`xy_vendor_probe()` 只探测、不 FATAL。引擎内核不需要 vendor，
依赖第三方的模块（Math / Decoder / Input）自己判断 `TARGET` 在不在，不在就跳过。
目录按 `<workspace>/Vendor` 与 `<workspace>/Modules` 的形状找，大小写都认。

### 做了什么

**host 表追加了服务表**（遵守「只尾部追加」，`size` 从 40 变 64）：

```c
XyAddServiceFn    add_service;      /* 重名返回非 0 */
XyGetServiceFn    get_service;      /* 找不到返回 NULL */
XyRemoveServiceFn remove_service;
```

配合 `XY_HOST_HAS(host, field)` 宏：老模块能用新引擎，新模块在老引擎上会自己降级。

**归属由引擎判定，模块说不了谎**：`host->add_service` 不看模块自报的名字，
而是读 `EngineContext::currentModule`——引擎在调用 `xy_module_v1` 前后设置它。

**卸载流程把「悬垂」变成可检测的**：

```
1. 调用 xy_module_shutdown_v1（可选）
2. 反查该模块名下还剩下的服务 → 逐个强制移除并计泄漏
3. FreeLibrary
```

**顺序**：`Programs` 的 DLL 先卸载（作用域先析构），再逆序卸载模块。
反过来的话，程序对象析构时会去摸已经 FreeLibrary 掉的模块，必崩。

### 实测

| 场景 | 结果 |
| --- | --- |
| `--module Probe.dll --program HelloXy.dll` | 装载 → 注册 → 运行程序 → 反注册 → 卸载，exit 0，无泄漏报告 |
| `--module Leaky.dll`（故意不导出 shutdown） | `警告 Leaky 未导出 xy_module_shutdown_v1` + `泄漏 Leaky 注册的服务 'leaky.marker'（已强制移除）` |
| `--module Nope.dll` | 报错，exit 67 |
| 导出表 | `Probe.dll` 只有 `xy_module_v1` + `xy_module_shutdown_v1` 两个符号 |

第三条是这一轮的核心收获：**文档 §7 里排第一的风险（卸载后注册表悬垂）现在是引擎自动检测的**，不靠自觉。

### 踩到的坑（重要）

1. **`Set-Content` 把源文件写成 GBK**。PowerShell 5.1 的 `Set-Content` 默认 ANSI（本机 936），
   我用它改过一次 `ModuleRegistry.cpp`，中文立刻变成乱码字节，而编译仍按 `-finput-charset=UTF-8`，
   于是运行时打印 `未导�?`。
   **规矩：改源码只用编辑工具（写 UTF-8），不要用 PowerShell 的 `Set-Content` / `Out-File` 回写。**
   检查办法：用 `UTF8Encoding($false,$true)` 严格解码，抛异常就是坏了。
2. 模块里的 `HMODULE` 不能存成 `void*` 再直接传给 `FreeLibrary`，要有 `AsHandle()` 过渡；
   头文件里保留 `void*` 是为了不把平台类型漏进公共头。

### 还没做（下一步的真正岔路口）

DataStore / Decoder / Input 现在是 `lib`，因为一旦切 `SHARED` 就有两条路，必须先选：

| 方案 | 做法 | 代价 |
| --- | --- | --- |
| **(a) 走 host 服务表** | dll 只依赖 `XyHostV1`，不链 `Files`/`Memory` | 要给每个内建能力写接口，模块不能直接用 C++ 类型 |
| **(b) 链 engine 的 import lib** | engine 导出内建符号（`ENABLE_EXPORTS` + `--whole-archive`），dll 链 `xy_engine` | 简单，但 dll 与 engine 版本死死绑定，且内建一旦改名就全炸 |

ABI spike 证明 (b) 在 MinGW 下可行；`xy_api.h` 的服务表证明 (a) 也可行。
**这一条决定了后面所有模块的写法，得先定。**

---

## 2026-10-22（三）· 仓库分家 + 最小引擎跑通

### 仓库结构（已定）

| 仓库 | 角色 | 内容 |
| --- | --- | --- |
| `X_Y` | **引擎侧** | 引擎自身产物的源码：`Engine/`（xy_engine.exe，将来还有 xy.exe 构建器）、`Modules/`（引擎内建模块）、`cmake/` |
| `Modules`（原 `X_YB`） | **工具源码仓库** | 自己写的工具模块源码（UI / Render / Widget / Media / Network …），不是引擎的一部分 |

- `Tools/` 取消：构建器是引擎的一部分，源码直接放 `Engine/`，产物 `xy.exe` 也从这里出。
- 改名有一个**真实副作用**：vendor 回退路径原本指向 `../X_YB/vendor`，改名后直接断掉（configure 报「找不到 vendor」）。已修成候选列表 + 命中外部路径时显式告警：`vendor/` → `../Modules/vendor` → `../X_YB/vendor`。
  **这不是长久之计**：引擎仓库应当自带 vendor（glm 1.6MB、stb 11MB、FFmpeg 头 9.5MB + `.a` 22MB，总共约 44MB，不含 FFmpeg 的 136MB 源码）。
- 空壳 `D:\workbench\X_YB` 目录已删。

### B1 最小运行器（已跑通）

新增：

```
Engine/include/xy_api.h      引擎与模块/程序之间唯一的 ABI 契约（C 头，C/C++ 共用）
Engine/src/Host.{h,cpp}      host 表的日志 / 分配器实现
Engine/src/main.cpp          xy_engine.exe：解析参数 → LoadLibrary → 调入口 → 退出
Engine/CMakeLists.txt        xy_program(xy_engine EXPORTS)
Programs/HelloXy/            最小程序 DLL，只认 xy_api.h，不链引擎
```

ABI 契约的三条硬规则（写在 `xy_api.h` 顶部）：

1. 跨边界只传 C 类型（POD / 函数指针 / C 字符串），不传 `std::string`、容器、异常。
2. 内存成对：`host->alloc` 发的只由 `host->free` 还。
3. 结构体只在**尾部追加**字段，使用方先看 `size` 再读；`abi_version` 不等就拒绝加载。

`XyHostV1` 目前 40 字节：`size / abi_version / user / log / alloc / free`。

### 实测结果

| 项 | 结果 |
| --- | --- |
| `xy_engine --version` | `abi=1 host_size=40` |
| `xy_engine --program HelloXy.dll a b c` | 日志打印 + argv 透传 + `host->alloc/free` 走一遍，exit 0 |
| 缺 `--program` | 打印用法，exit 64 |
| 载入不存在的 dll | 报错码 126，exit 65 |
| **严格导出** | `HelloXy.dll` 导出表**只有 1 个符号**：`xy_program_v1` |

最后一条是本轮最重要的验证：`-Wl,--exclude-all-symbols` 生效，模块内部符号（含 `std::` 实例化）全部不导出。这意味着之前担心的「两个 dll 符号冲突」在结构上被堵死——每个 dll 只交出一个入口名。

### 踩到的编译坑

- `FreeLibrary` / `GetProcAddress` 要 `HMODULE` 不是 `void*`，句柄得用平台原生类型（`NativeHandle`）。
- `alignof(std::max_align_t)` 需要 `<cstddef>`。
- `_aligned_malloc` 必须配 `_aligned_free`（`malloc` 的指针不能交给 `_aligned_free`）。Hash 到的结论：**分配器只留一条路径**——Windows 一律 `_aligned_malloc`，POSIX 一律 `posix_memalign`，这样 `HostFree` 不需要知道调用方的 `align`。

### 下一步

B2：模块装载。用 `Modules/` 里的真实模块（先 `Timer` 这种无依赖的）验证 `xy_module_v1` 装载 + 卸载，重点看**卸载后静态注册表有没有悬垂指针**——这是文档里列的头号风险。

---

## 2026-10-22（二）· dll / exe / lib 三类产物 + 平台接线下方到模块

### 需求

要能产出 **lib / dll / exe** 三类东西，并且跨平台——跨平台那部分写在**各模块自己的 CMakeLists** 里，根不要长满 `if(WIN32)`。

### 结构（根只留秩序）

```
CMakeLists.txt              全局选项 + 依赖顺序 + 进目录
cmake/XYModule.cmake        xy_module / xy_program / 产物属性
cmake/XYVendor.cmake        xy_vendor_glm / _stb / _ffmpeg 接线目标
Modules/<Name>/CMakeLists.txt   本模块的依赖 + 平台差异
Engine/CMakeLists.txt       （将来）xy_program(xy_engine EXPORTS ...) → xy_engine.exe
Tools/xy/CMakeLists.txt     （将来）xy.exe
```

根 CMakeLists 里的 `foreach(_d Engine Tools/xy)` 用「目录里有 CMakeLists.txt 才接进来」的方式，所以现在这两个目录是空的也不会报错。

### 三类产物

| 类型 | 写法 | 产物 |
| --- | --- | --- |
| lib | `xy_module(Foo)`（`XY_MODULE_LINKAGE=STATIC`） | `out/lib/libFoo.a` |
| dll | `xy_module(Foo)` + `-DXY_MODULE_LINKAGE=SHARED`，或 `xy_module(Foo SHARED)` | `out/bin/Foo.dll` + `out/lib/libFoo.dll.a` |
| exe | `xy_program(App DIR <dir> [EXPORTS])` | `out/bin/App.exe` |

- `EXPORTS` 对应文档里 `ENABLE_EXPORTS TRUE` 的用法：**engine 这种 exe 也要导出符号**，模块 dll 才能反向链它。
- dll 在 Windows 上 `PREFIX ""`（产物名就是身份），MinGW 默认 auto-export 全部符号，所以开了 `XY_STRICT_EXPORTS` → `-Wl,--exclude-all-symbols`；Linux 侧用 `-fvisibility=hidden`。
- `Memory` 强制 STATIC，不受全局开关影响：它重载了全局 `operator new/delete`，全进程只能有一份。
- 新增配置期告警：某个 dll 静态链了静态库时提醒「那个库的全局状态会在每个 dll 里各一份」。

### 平台接线长什么样（以 Input 为例）

```cmake
if(NOT WIN32)
    message(STATUS "Input: 当前只有 Windows 后端（Win32 消息 + WASAPI），本平台跳过")
    return()
endif()
xy_module(Input DEPS Decoder)
target_link_libraries(Input PUBLIC ole32 uuid user32)   # WASAPI / COM
```

Decoder / Files / DataStore 同理，被跳过的模块会在下游链式 `return()` 并打印原因，**不留假的空 target**（否则错误会推迟到链接期，更难查）。

### 验证（configure 级，按约定不编译）

- STATIC：`out/lib/libCore.a … libInput.a`（9 个）
- `-DXY_MODULE_LINKAGE=SHARED`：出现 `out/bin/Core.dll … Input.dll` 与 `out/lib/lib*.dll.a`，`Memory` 仍只出 `libMemory.a`
- 临时工程（已删）验 `xy_program`：`out/bin/smoke_app.exe`，链接行里有 `out/lib/libCore.dll.a`

### 顺带记住的坑

`ninja -n` 干跑在有 `CONFIGURE_DEPENDS` 时会一直显示「Re-running CMake」（ninja 干跑无法得知 glob 检查结果），要验证构建图得用 `ninja -t targets` / `ninja -t commands`。

---

## 2026-10-22 · 引擎构建系统落地（第一步）

### 背景

`_notes/REFACTOR_PLAN.md` 描述的 14 模块 / UI 结构已经和磁盘不符：`X_Y` 现在是 9 个精简模块（Core / Memory / Timer / Math / Log / Files / DataStore / Decoder / Input），
`Engine/`、`cmake/`、`test/`、`Tools/` 都是空的，全工程 **一个 CMakeLists.txt 都没有**，唯一的可构建物是 `_notes/spike/abi`。
所以第一步不是写 engine，而是**先让这 9 个模块能编**。

### 做了什么

- 新增 `CMakeLists.txt`（根）
  - C++20、`-finput-charset=UTF-8 -fexec-charset=UTF-8`（MSVC 侧 `/utf-8`）、`XY_PLATFORM_*`、`XY_DEBUG/XY_RELEASE`
  - 产物统一到 `build/out/lib`（静态库）与 `build/out/bin`（将来的 exe）
  - `XY_VENDOR_DIR` 自动探测：工程内 `vendor/` → `../X_YB/vendor`，可用 `-DXY_VENDOR_DIR=` 覆盖
  - FFmpeg 用 `X_YB/vendor/FFmpeg` 里**已编好的** `.a`，不再跑 ExternalProject 源码构建
  - `XY_MODULES` 选择器：只编指定模块 + 其依赖闭包（如 `-DXY_MODULES=Log` → `Log;Core;Timer`）
- 新增 `cmake/XYModule.cmake`
  - `xy_module(<Name> [HEADER_ONLY] [DEPS ...] [EXCLUDE ...])`：`Modules/<Name>/src/**.cpp` 自动收集，公开头 = 模块根下除 `src/` 外的一切头
  - 记录 `XY_PUBLIC_HEADERS` 属性 → 将来 `xy store` 打包 `include/` 时直接用，不用再维护清单
  - include 根 = 模块根（`#include "Timer.h"`）+ `Modules`（`#include <Core/Core.h>`）

### 实测结论（侦察得到的事实，和旧文档不同）

- **Log 和 Math 是 header-only**：Log 只有 `Log.h`（类全 inline），Math 只有三个头。文档里"每模块一个 DLL"对这两个不成立，它们只能当 builtin。
- 真实依赖图很小：`Log→Core,Timer`、`Files→Memory`、`Decoder→Core,Memory,Files(+FFmpeg,stb)`、`Input→Decoder`、`DataStore→Memory,Files`。文档里的"151 处跨模块 include"在 X_Y 已不存在。
- `Decoder/Decoder.h` 是公开头却 include 了 `<windows.h>` —— 违反"平台头只在 engine 内"，待修。
- include 环：`Decoder.h → Files.h`，而 `Files/src/FilesSystem.cpp → Decoder.h`。好在 `Decoder.h` 里没用 `Files::`，且 `Decoder::Decode()` 是头内模板，**链接图无环**，所以静态库能编；但将来模块化 DLL 时必须拆掉。
- `Memory/src/XMemGlobalNew.cpp` 重载全局 `new/delete`（只做统计）。静态库链进 engine 没问题，但**绝不能出现在可卸载的模块 DLL 里**。

### 验证

- `cmake -S . -B build -G Ninja` configure 通过（MinGW 13.1.0 / CMake 4.3.2）
- `ninja -t commands` 确认：9 个 target 齐、`out/lib/lib<Module>.a`、Decoder 拿到 `-I.../X_YB`（stb）与 `-I.../vendor/FFmpeg`
- 选择器实测：`-DXY_MODULES=Log` → 只构建 `Log;Core;Timer`
- **未做**：全量 `cmake --build`（按约定交给砚台跑）

### 下一步

手工编 `HelloXy.dll` + 最小 `xy_engine.exe`：`LoadLibrary` → 调 `xy_program_v1(host)` → 退出。
顺手验证：`-Wl,--exclude-all-symbols` 能否让模块只导出入口符号（`objdump -p xxx.dll | findstr /i export`），以及 `XyHost` 结构体 + 首字段 size 的版本化写法。
