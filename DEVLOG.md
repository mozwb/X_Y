# X_Y DEVLOG

> 最新在上。设计文档见 `_notes/REFACTOR_PLAN.md`。

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
