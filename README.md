# X_Y

X_Y 是一个运行期引擎：`X_Y` 负责装载 DLL/SO、收集命令、执行命令和外部程序；
`pkg` 管已构建的包；`xy` 负责构建和从仓库安装。

> **这份 README 是当前实现的契约**：目录、命令、默认 root、ABI 以这里为准。
> 改代码必须同步改 README；“未完成 / 下一步”是规划，不算已实现。
> 版本：**1.1.0-dev**（基于 1.0.0，增加服务模块入口）。最后核对：2026-10-31。

![设计图](./X_Y设计图.jpg)

> 代码地图见 [`_notes/KERNEL_MAP.md`](_notes/KERNEL_MAP.md)，
> Linux 验收清单见 [`_notes/LINUX_VERIFY.md`](_notes/LINUX_VERIFY.md)。

---

## 1. 用户使用目录

用户拿到的是**运行时包**，不需要源码、不需要构建器、不需要头文件。

### Windows

```
X_Y/
├─ bin/
│  ├─ X_Y.exe
│  ├─ pkg.dll
│  ├─ Memory.dll
│  ├─ libstdc++-6.dll
│  ├─ libgcc_s_seh-1.dll
│  └─ libwinpthread-1.dll
│
├─ modules/
│  └─ HelloXy/
│     └─ 0.1.0/
│        ├─ package.xy
│        └─ HelloXy.dll
│
└─ README.md
```

### Linux

```
X_Y/
├─ bin/
│  ├─ X_Y
│  ├─ pkg.so
│  └─ libMemory.so
│
├─ modules/
│  └─ HelloXy/
│     └─ 0.1.0/
│        ├─ package.xy
│        └─ HelloXy.so
│
└─ README.md
```

### 约定

- `X_Y` 和 `pkg` 永远在 `bin/` 同目录。
- `Memory.dll` / `libMemory.so` 也在 `bin/`；X_Y、IO、consumer 共用这一份。
- 所有可加载的包/模块都在 `modules/<name>/<version>/`。
- `pkg` 默认 root 是 `bin/../modules`；`X_Y` 启动时会把 `XY_MODULES` 设成这个路径。
- 用户包里没有 `xy`、`include/`、`cache/`、`repo/`、源码。
- Windows 运行库放在 `bin/`，Linux 用系统的 `libstdc++`。

### 用户命令

```
X_Y                                  交互 shell
X_Y --run abi                        执行一条命令
X_Y --module bin/pkg.dll             启动时装载 pkg
X_Y --program modules/HelloXy/0.1.0/HelloXy.dll a b
```

shell 里：

```
> load pkg.dll
> pkg list
> pkg install /path/to/package-dir
> load HelloXy.dll
> run HelloXy a b
```

---

## 2. 开发使用目录

开发包才有源码、构建器、头文件和缓存。

```
X_Y/
├─ CMakeLists.txt               # 内核 + Apps + Programs + Builder
├─ enve.json                    # 构建环境：root/bin/modules/lib/temp/source
├─ README.md
├─ X_Y设计图.jpg
├─ .gitignore
│
├─ Engine/                      # 内核源码
│  ├─ include/
│  │  ├─ xy_api.h               # ABI：版本 / host 表 / program 入口
│  │  └─ xy_command.h           # 命令形状 / commands 入口
│  └─ src/
│     ├─ main.cpp               # 启动、参数、模式
│     ├─ Host.h / Host.cpp      # Engine + host 回调
│     ├─ EngineContainer.h/.cpp # 单线程容器：create/load/run/share/unload
│     ├─ Services.h / .cpp      # 名字 → 指针 + owner
│     ├─ CommandTable.h / .cpp  # 命令名 → XyCommandV1* + owner
│     ├─ ModuleRegistry.h/.cpp  # DLL 装载 / ABI 检查 / 卸载
│     ├─ KernelCommands.h/.cpp  # 内建命令
│     ├─ Shell.h / Shell.cpp    # 分词 / run 分发 / 交互循环
│     └─ Platform/
│        ├─ Library.h           # LoadLibrary / dlopen 接口
│        ├─ Process.h           # CreateProcess / fork 接口
│        ├─ Win32/
│        │  ├─ LibraryWin32.cpp
│        │  └─ ProcessWin32.cpp
│        └─ Posix/
│           ├─ LibraryPosix.cpp
│           └─ ProcessPosix.cpp
│
├─ Apps/
│  └─ Pkg/                      # 包管理命令 DLL
│     ├─ CMakeLists.txt
│     └─ src/main.cpp
│
├─ Builder/                     # xy.exe：构建 + 仓库安装
│  ├─ CMakeLists.txt
│  └─ src/main.cpp
│
├─ Programs/
│  └─ HelloXy/                  # 最小微程序
│     ├─ CMakeLists.txt
│     ├─ package.xy
│     └─ src/main.cpp
│
├─ test/                        # 容器冒烟测试
│  ├─ container_smoke.ps1
│  ├─ container_smoke.sh
│  └─ fixtures/
│     ├─ echo_api.h
│     ├─ echo_service.c
│     └─ echo_client.c
│
├─ _notes/                      # 不纳入版本控制
│  ├─ KERNEL_MAP.md
│  ├─ LINUX_VERIFY.md
│  └─ REFACTOR_PLAN.md
│
├─ build/                       # 构建树（不进版本控制）
│  └─ out/
│     ├─ bin/                   # X_Y、xy、pkg、HelloXy、Memory.dll
│     ├─ lib/                   # .a / .lib / import lib
│     └─ modules/
│        ├─ Memory/              # 第三方依赖包：include + bin + lib
│        ├─ IO/                  # 第三方 C++ 库：include + bin + lib
│        └─ ...                  # pkg install 的安装目标
│
├─ .cache/                      # xy build 的临时产物（不进版本控制）
│  └─ out/
│
└─ repo/                        # 可选：本地包仓库
   └─ HelloXy/0.1.0/...
```

### 构建目录 vs 发布目录

| 开发构建 | 发布/用户 |
|---|---|
| `build/out/bin/` | `bin/` |
| `build/out/modules/` | `modules/` |
| `build/out/lib/` | 不发布（只放编译期库） |
| `.cache/` | 不发布 |
| `_notes/`、`Engine/`、`Apps/`、`Builder/` | 不发布 |

Windows 的 `.dll`、Linux 的 `.so` 都在 `out/bin`；`out/lib` 只放静态库/import lib。

---

## 3. 文件职责

| 文件 | 负责 |
|---|---|
| `Engine/include/xy_api.h` | ABI 底座：版本、导出宏、host 表、program 入口 |
| `Engine/include/xy_command.h` | 命令形状、commands 入口 |
| `Engine/src/Host.*` | `Engine`：services / commands / modules / host，以及 alloc/free/log/service 回调 |
| `Engine/src/EngineContainer.*` | 单线程容器：root + 子 Engine 的 create/list/destroy/load/unload/run/services/shared/share/unshare |
| `Engine/src/Services.*` | 名字 → 指针 + owner |
| `Engine/src/CommandTable.*` | 命令名 → 命令对象 + owner |
| `Engine/src/ModuleRegistry.*` | DLL 装载/卸载、ABI 检查、命令登记 |
| `Engine/src/KernelCommands.*` | `help/commands/modules/engine/load/unload/program/exec/mem/abi` |
| `Engine/src/Shell.*` | 分词、`run` 前缀、查表、owner、交互循环 |
| `Engine/src/main.cpp` | 参数、启动、模式选择 |
| `Engine/src/Platform/*` | 动态库和外部进程的平台差异 |
| `Apps/Pkg` | `pkg list/info/verify/install/remove` |
| `Builder` | `xy build/module/install/doctor` |
| `Programs/HelloXy` | 只认 `xy_api.h` 的最小微程序 |
| `test/` | 容器冒烟测试和 echo fixture |
| `CMakeLists.txt` | 目标、输出目录、Memory 共享库、程序/应用接入 |

---

## 4. 构建

### 前置

- C++20 编译器（MinGW g++ 13 / Linux g++）
- CMake >= 3.24
- Ninja 或 Make
- `Modules/` 里先编出共享的 `Memory.dll` / `libMemory.so` 和 import lib

### Linux

```bash
cd /path/to/workbench

cmake -S Modules -B Modules/build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DXY_MODULES=Memory
cmake --build Modules/build-linux --target Memory

cmake -S X_Y -B X_Y/build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DXY_MEMORY_LIB="$PWD/Modules/build-linux/out/lib/libMemory.so" \
  -DXY_MEMORY_DLL="$PWD/Modules/build-linux/out/lib/libMemory.so"
cmake --build X_Y/build-linux
```

产物：

```
X_Y/build-linux/out/bin/X_Y
X_Y/build-linux/out/bin/xy
X_Y/build-linux/out/bin/pkg.so
X_Y/build-linux/out/bin/HelloXy.so
X_Y/build-linux/out/bin/libMemory.so
```

### Windows（MinGW Makefiles）

```powershell
$env:PATH = "D:\QT\Tools\mingw1310_64\bin;" + $env:PATH

cmake -S X_Y -B X_Y/build -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_CXX_COMPILER="D:/QT/Tools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="D:/QT/Tools/mingw1310_64/bin/mingw32-make.exe" `
  -DXY_MEMORY_LIB="D:/workbench/Modules/build/out/lib/libMemory.dll.a" `
  -DXY_MEMORY_DLL="D:/workbench/Modules/build/out/bin/Memory.dll"

cmake --build X_Y/build
```

> 这台机器上 Ninja 的 `try_compile` / glob recheck 会卡，Windows 构建固定用 MinGW Makefiles。

---

## 5. pkg

`pkg` 是命令 DLL，管理 `modules/`。

```
pkg list   [root] [--repo <dir>]
pkg info   <name> [root] [--repo <dir>]
pkg verify [root] [--repo <dir>]
pkg install <源目录> [--root <dir>] [--force]
pkg remove  <name> [version] [--root <dir>] [--all]
```

默认 root 顺序：

1. 命令行 `--root` / 位置参数
2. 环境变量 `XY_MODULES`
3. `./Modules`

`X_Y` 启动时会把 `XY_MODULES` 设成 `bin/../modules`，所以正常运行时不用手写 root。

manifest：

```
name    = HelloXy
version = 0.1.0
kind    = program
abi     = 1
entry   = xy_program_v1
file    = HelloXy.dll
source  = src
```

安装布局：

```
modules/<name>/<version>/
  package.xy
  <包内文件>
```

### Engine 容器

`engine` 是内建命令，管理子 Engine：

```
engine list
engine create <name>
engine load <name> <dll>
engine unload <name> <module>
engine run <name> <命令...>
engine services <name>
engine shared
engine share <name> <service>
engine unshare <service>
engine destroy <name>
```

每个子 Engine 有自己的 host / services / commands / modules；`engine run` 只在该子 Engine 里查命令，不会串到 root。

```bash
# 装 / 跑：HelloXy 是当前真实存在的包
engine create a
engine load a modules/HelloXy/0.1.0/HelloXy.dll
engine run a HelloXy a b
engine destroy a

# 共享：假设 net 包在它的 xy_module_v1 里注册了 "net" 服务
engine create net
engine load net modules/net/0.1.0/net.dll
engine share net net
engine create app
engine load app modules/app/0.1.0/app.dll
engine run app ...
engine unshare net
engine destroy net
```

服务查找规则：

- 所有 Engine：先查本地，再查 `container.shared`
- 子 Engine 本地和 shared 都没有，最后查 root
- root 不反向看子 Engine；兄弟之间不直接互看
- `engine share` 把 provider 的本地服务提升到 shared
- `engine services <name>` 看本地服务；`engine shared` 看共享表和 provider
- provider 还有共享服务时不能 `engine destroy`，也不能 `engine unload` 它的模块
- root 有共享服务时，`unload` 同样被拒绝
- 先 `engine unshare`，再销毁/卸载

---

## 6. xy

`xy` 是构建期工具，不进用户包。

```
xy build   <源目录> [--out <目录>] [--include <Engine/include>] [--cxx <编译器>]
xy module  <name> [--modules-root <Modules/Modules>] [--out <目录>] [--cxx <编译器>]
xy install <name> [version] [--repo <仓库>] [--root <安装根>]
           [--build] [--force] [--include ...] [--cxx ...]
           [--x-y <X_Y>] [--pkg <pkg>]
xy doctor  [--repo <仓库>] [--include ...] [--cxx ...]
xy env
```

### Modules 构建

`xy module` 直接读 Modules 侧的 `module.xy`，先建依赖，再建自己：

```bash
xy module IO
```

`Modules/Modules/IO/module.xy`：

```
name = IO
version = 0.0.1
kind = library
sources = src
include = ..
headers = .
exclude = src/FilesSystem.cpp
deps = Memory
libs =
defines =
```

流程：

```
xy module IO
  ├─ 读 Modules/Modules/IO/module.xy
  ├─ deps = Memory
  │  └─ xy module Memory -> out/modules/Memory
  └─ 编 IO -> out/modules/IO
```

产物：

```
out/modules/IO/
├─ include/IO/{Files.h,Stream.h}
├─ bin/IO.dll
├─ lib/libIO.dll.a
└─ package.xy
```

`--modules-root` 默认找 `../../../../Modules/Modules` 或 `./Modules/Modules`；
`--out` 默认 `<xy>/../modules`。

`xy install IO` 也会先查 Modules 侧的 `IO/module.xy`，有就直接走依赖构建，不要求 `package.xy` 仓库。

### 构建环境 enve.json

`root` 就是 `out` 目录；`bin` / `modules` / `lib` / `temp` 默认都相对它：

```json
{
    "root": "build/out",
    "bin": "bin",
    "modules": "modules",
    "lib": "lib",
    "temp": ".cache/tmp",
    "source": "../Modules/Modules"
}
```

- `root`：输出根，相当于 `out/`
- `bin`：可执行文件和运行库
- `modules`：模块/包安装目录
- `lib`：库目录
- `temp`：编译器和链接器的临时目录
- `source`：Modules 源码根

`xy env` 会打印当前解析结果：

```powershell
PS X_Y> ./build/out/bin/xy env
config  = D:\workbench\X_Y\enve.json
root    = D:\workbench\X_Y\build\out
...
```

相对路径按 `enve.json` 所在目录解析；不写 `root` 时默认是配置文件所在目录。
`xy` 启动时会把 `TMP` / `TEMP` / `TMPDIR` 指到 `temp`，编译器和链接器的临时文件不再落到系统 TEMP。

### 仓库

```
<repo>/
  HelloXy/
    0.1.0/
      package.xy
      HelloXy.dll      # 二进制包
      src/...          # 源码包（可选）
```

- `file` 存在 → 二进制包
- `source` 目录存在 → 源码包
- 版本省略 → 选最高版本
- 有二进制默认用二进制；`--build` 强制源码

### 流程

```
xy install HelloXy 0.1.0 --repo repo/
  ├─ 在 repo 找包
  ├─ 有二进制 -> X_Y --module pkg --run pkg install <包目录>
  └─ 只有源码 / --build -> xy build -> pkg install
```

`xy` 自己不写 `modules/`，收包始终由 `pkg` 做。
`--root` 默认 `bin/../modules`，和 `pkg` 的默认 root 一致。

### 自检

```
xy doctor --repo ../Modules
```

检查：编译器、Engine include、X_Y、pkg、repo。

### 容器冒烟测试

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File X_Y\test\container_smoke.ps1
```

```bash
bash X_Y/test/container_smoke.sh
```

`container_smoke` 会编 `echo_service` / `echo_client` 到 `build/test-fixtures`，跑：
create → load → services → run → share → shared → unload 拦截 → unshare → unload → destroy。

---

## 7. 扩展与 ABI

普通扩展只 include：

```cpp
#include <xy_api.h>
#include <xy_command.h>
```

入口：

| 导出 | 用途 |
|---|---|
| `xy_abi_version_v1` | 必须导出，返回 `XY_ABI_VERSION` |
| `xy_program_v1` | 微程序入口 |
| `xy_commands_v1` | 命令表入口 |
| `xy_module_v1` | 可选：模块初始化，注册服务 |
| `xy_module_shutdown_v1` | 可选：卸载前反注册服务 |

加载顺序：验 ABI → `xy_module_v1(host)` → `xy_commands_v1(host)` → 登记命令。
卸载顺序：`xy_module_shutdown_v1()` → 摘命令 → 摘服务 → `FreeLibrary`。

规则：

- 跨 DLL 只传 C 类型；不传 `std::string` / 容器 / 异常
- 结构体只尾部追加；用 `size` 判断字段
- `host->alloc` 的指针只能 `host->free`
- 命令表、命令对象归 DLL 所有，卸载前必须有效
- 一次性 program 禁止 `add_service`
- 服务在 `xy_module_v1` 注册，`xy_module_shutdown_v1` 反注册；漏掉的由内核按 owner 清理并报泄漏

### Memory 第三方库

`Memory` 现在是共享库，不再是 X_Y 整库链进去的静态库：

```cpp
#include <Memory/Memory.h>

auto* w = X_Y::New<Widget>(args...);
X_Y::Delete(w);

void* p = X_Y::Malloc(1024);
X_Y::Free(p);

X_Y::Buffer buffer;
```

- `New` / `Delete` 成对，`Malloc` / `Free` 成对
- 不再重载全局 `operator new/delete`；裸 `new` 不归 Memory 统计
- `mem` 的“门面”只统计显式 Memory API；“进程”仍是 OS 口径的 RSS / private
- `Memory.dll` 必须和 X_Y / IO / consumer 一起分发；一个进程共用一份

Memory 包在 `out/modules/Memory/`：

```
Memory/
├─ include/Memory/Memory.h
├─ bin/Memory.dll
├─ lib/libMemory.dll.a
└─ package.xy
```

X_Y 的 `Engine/include` 不复制 `Memory.h`；consumer 的 include 路径是：

```
-I X_Y/Engine/include
-I out/modules/Memory/include
```

### IO 第三方库

IO 不是命令、不是服务，是 C++ 共享库，构建到：

```
out/modules/IO/
├─ include/IO/        # Files.h / Stream.h
├─ bin/IO.dll         # libIO.so
└─ lib/libIO.dll.a    # import lib
```

consumer：

```cpp
#include <IO/Stream.h>
#include <IO/Files.h>

X_Y::FileStream file("a.bin");
X_Y::Buffer data = X_Y::FilesSystem::ReadFileBinary("a.bin");
```

链接：

```
-I out/modules/IO/include
-L out/modules/IO/lib -lIO
-L .../Memory -lMemory
```

- `IO.dll` 依赖 `Memory.dll`，不依赖 `X_Y.exe`
- 当前 `IO.dll` 包含 `Stream` + 跨平台文件/目录核心（`Modules/IO/src/FilesCore.cpp`）
- Windows 文件对话框（`OpenFileDialog/SaveFileDialog`）依赖 Decoder/UI，暂留在 `FilesSystem.cpp`，不进 `IO.dll`
- 运行期还要让 loader 找到 `out/modules/IO/bin`；目前测试用 PATH，后面在 `LibraryWin32` 里加模块搜索路径
- 构建入口是 `xy module IO`，不再是 X_Y 的 CMake 目标

`XyHostV1` 是内核给扩展的能力表；它只放 alloc/free/log/service。
网络、文件这类能力以后通过 **服务对象**（每个 engine 自己的 host + C 函数表）注入，
不往全局 host 上堆。

---

## 8. 当前状态

v1.1.0-dev 已完成：

- ABI 头、host、services、command table
- 服务模块入口：`xy_module_v1` / `xy_module_shutdown_v1`
- Engine 实例化：每个 Engine 一份 host / services / commands；`ModuleRegistry` 也归 Engine 所有（`Start()` 里建）
- EngineContainer：单线程 `engine create/list/destroy/load/unload/run/services/shared/share/unshare`
- 容器共享服务：`share` / `unshare`，provider 共享期间禁止销毁/卸载
- 容器冒烟测试：`test/container_smoke.ps1` / `container_smoke.sh`
- Memory 共享库：`Memory.dll` / `libMemory.so`，X_Y 不再整库链 Memory
- Memory 依赖包：`out/modules/Memory/{include,bin,lib,package.xy}`
- Memory 显式 API：`X_Y::New/Delete/Malloc/Free/Buffer`；全局 new 不再覆盖
- IO 第三方 C++ 库：`out/modules/IO/{include,bin,lib,package.xy}`，链 IO + Memory
- `pkg` 接受 `kind = library`，list/verify 能识别 Memory/IO
- 跨 engine 服务查找：本地 → container.shared → root（子 Engine）
- DLL 装载/卸载、微程序包装、命令 owner
- 内建命令、shell、main
- Windows 平台 Library / Process 实测
- Linux 真机验收（用户确认已跑；验收输出未留档）
- `pkg` v1：list / info / verify / install / remove
- `xy` v1：build / module / install / doctor，仓库按 name/version 安装；`install` 可识别 Modules 侧 `module.xy`
- `enve.json` 环境：`root` = out，bin/modules/lib/temp/source 统一解析
- 固定运行目录：`out/bin` + `out/modules`，`X_Y` 自动设置 `XY_MODULES`

未完成 / 下一步：

- 懒启动 / 服务引用计数 / 线程
- 更多服务对象 ABI（`net_api.h` 这类）
- `provides` / `requires` 依赖顺序
- `pkg` 联网下载（HTTP + 索引 + 校验）
- 多线程策略（放 Modules，内核 v2 接入）
- `xy doctor` 对 DLL 导出表的检查

---

## 9. README 维护规矩

- 改目录布局、默认 root、命令参数、ABI 字段：**同一次改动里更新本文件**。
- 新功能先加到“已完成”，不要只留在 `_notes`。
- “未完成 / 下一步”只是方向，不当作已经存在的能力。
- 如果代码和本文件冲突，以本文件为准，先改代码或先改文件，不能两边各说各的。
