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
│  │  ├─ xy_command.h           # 命令形状 / commands 入口
│  │  └─ xy_kernel.h            # kernel 命令分发服务
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
├─ Plugins/
│  └─ Shell/0.1.0/              # X_Y 专用插件：Modules Shell + kernel 回调
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
|     ├─ .cache/                # xy build 的临时产物（不进版本控制）
│     └─ modules/
│        ├─ Memory/              # 第三方依赖包：include + bin + lib
│        ├─ IO/                  # 第三方 C++ 库：include + bin + lib
│        └─ ...                  # pkg install 的安装目标
│
│
└─ repo/                        # 可选：本地包仓库
   └─ HelloXy/0.1.0/...
```

### 构建目录 vs 发布目录

| 开发构建                                  | 发布/用户              |
| ----------------------------------------- | ---------------------- |
| `build/out/bin/`                          | `bin/`                 |
| `build/out/modules/`                      | `modules/`             |
| `build/out/lib/`                          | 不发布（只放编译期库） |
| `.cache/`                                 | 不发布                 |
| `_notes/`、`Engine/`、`Apps/`、`Builder/` | 不发布                 |

Windows 的 `.dll`、Linux 的 `.so` 都在 `out/bin`；`out/lib` 只放静态库/import lib。

---

## 3. 文件职责

| 文件                           | 负责                                                                                              |
| ------------------------------ | ------------------------------------------------------------------------------------------------- |
| `Engine/include/xy_api.h`      | ABI 底座：版本、导出宏、host 表、program 入口                                                     |
| `Engine/include/xy_command.h`  | 命令形状、commands 入口                                                                           |
| `Engine/include/xy_kernel.h`   | `kernel` 服务：`run(argc, argv)` 命令分发                                                          |
| `Engine/src/Host.*`            | `Engine`：services / commands / modules / host，以及 alloc/free/log/service 回调                  |
| `Engine/src/EngineContainer.*` | 单线程容器：root + 子 Engine 的 create/list/destroy/load/unload/run/services/shared/share/unshare |
| `Engine/src/Services.*`        | 名字 → 指针 + owner                                                                               |
| `Engine/src/CommandTable.*`    | 命令名 → 命令对象 + owner                                                                         |
| `Engine/src/ModuleRegistry.*`  | DLL 装载/卸载、ABI 检查、命令登记                                                                 |
| `Engine/src/KernelCommands.*`  | `help/commands/modules/engine/load/unload/program/exec/mem/abi`                                   |
| `Engine/src/Shell.*`           | 分词、`run` 前缀、查表、owner、交互循环                                                           |
| `Engine/src/main.cpp`          | 参数、启动、模式选择                                                                              |
| `Engine/src/Platform/*`        | 动态库和外部进程的平台差异                                                                        |
| `Apps/Pkg`                     | `pkg list/info/verify/install/remove`                                                             |
| `Builder`                      | `xy build/module/install/doctor`；构建 DLL 与独立 EXE                                           |
| `Programs/HelloXy`             | 只认 `xy_api.h` 的最小微程序                                                                      |
| `Plugins/Shell`                | Modules Shell + `kernel` 回调，作为 `xy_program_v1` 插件                                         |
| `test/`                        | 容器冒烟测试和 echo fixture                                                                       |
| `CMakeLists.txt`               | 目标、输出目录、Memory 共享库、程序/应用接入                                                      |

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
engine share-command <name> <command>
engine unshare-command <command>
engine shared-commands
engine destroy <name>
```

每个子 Engine 有自己的 host / services / commands / modules；`engine run` 的查找顺序和服务一致：
**本地 → container.shared → root**。

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

命令查找规则相同：

- 本地 `commands`
- `container.shared` 命令（`engine share-command`）
- 子 Engine 最后查 root 命令
- `engine shared-commands` 看共享命令和 provider
- `unshare-command` 之后才能销毁/卸载 provider
- root 有共享服务时，`unload` 同样被拒绝
- 先 `engine unshare`，再销毁/卸载

---

## 6. xy

`xy` 是构建期工具，不进用户包。`xy build` 直接调用 C++ 编译器，不要求应用源码使用 CMake。

```
xy build   <源目录> [--out <目录>] [--include <Engine/include>] [--cxx <编译器>]
xy module  <name> [version] [--link auto|source|static|shared]
           [--repo <模块仓库>] [--out <modules 目录>] [--cxx <编译器>]
xy install <name> [version] [--repo <仓库>] [--root <安装根>]
           [--build] [--force] [--include ...] [--cxx ...]
           [--x-y <X_Y>] [--pkg <pkg>]
xy doctor  [--repo <仓库>] [--include ...] [--cxx ...]
xy env
```

`package.xy` 的 `kind` 决定链接类型：

- `program` / `module`：按共享库构建，继续用于 X_Y 的 DLL/SO 装载。
- `executable`：按独立进程构建，不需要 `Engine/include`；产物可通过 X_Y 的 `exec` 命令启动。

`xy build` 只构建清单中声明的项目，不会联网或从源码仓库隐式获取依赖。开发模块必须先用 `xy module` 准备到 `enve.json` 的 `modules` 目录。

例如独立程序的清单：

```
name = HelloTool
version = 1.0.0
kind = executable
abi = 1
file = bin/HelloTool.exe
source = src
```

源码放在 `src/` 中，执行：

```powershell
xy build path/to/HelloTool --out build/out
X_Y --run exec build/out/HelloTool/1.0.0/bin/HelloTool.exe arg1 arg2
```

`file` 是相对包目录的输出路径；Linux 下也可以使用不带 `.exe` 的文件名。`deps`、`libs`、`includes` 和 `libDirs` 对 DLL 与可执行程序都有效。

### 开发模块

```bash
xy module IO --link static
xy module Memory --link shared
xy module HeaderOnly --link source
```

模块仓库使用统一的 `package.xy` 清单，也兼容旧的 `module.xy`。模块清单示例：

```
name = IO
version = 0.0.1
kind = library
source = src
include = ..
headers = .
exclude = src/FilesSystem.cpp
deps = Memory:static
libs =
libs_windows =
libs_linux =
libDirs =
defines =
```

- `--link auto` 优先复用静态库，其次共享库；均不存在时从源码构建静态库。
- `--link source` 只准备源码和头文件；仍递归准备依赖模块的源码。
- `--link static` / `shared` 优先复用该类型的预编译产物，不存在时尝试从源码构建。
- 二进制模式只安装库和头文件；即使为了生成库而读取/编译了仓库源码，也不会把源码复制到输出目录。
- 依赖模块会递归准备；`xy build` 不会自动下载或编译未准备的依赖。
- `--repo` 指定模块源码仓库，默认取 `enve.json` 的 `source`；`--out` 指定模块安装目录，默认取 `enve.json` 的 `modules`。

准备后的开发模块使用统一目录布局：

```
out/modules/IO/
├─ include/IO/...
├─ bin/IO.dll                 # 选择 shared 时存在
├─ lib/libIO.a                # 选择 static 时存在
├─ lib/libIO.dll.a            # Windows shared 的 import library
├─ src/                       # 选择 source 或从源码构建时存在
└─ package.xy
```

`xy build` 的 `deps` 用于引用这些模块，例如 `deps = IO:static, Memory:shared`。`libs`、`libs_windows`、`libs_linux` 和 `libDirs` 用于声明第三方链接库，不会被当作 `xy module` 的源码模块。

### Shell 插件

`Plugins/Shell/0.1.0` 是第一个 X_Y 专用插件：

```
Plugins/Shell/0.1.0/
├─ package.xy        # kind=program, deps 带 static/shared
└─ src/main.cpp      # xy_program_v1 + Modules Shell + kernel fallback
```

构建安装：

```powershell
xy install Shell --repo Plugins --build
```

运行：

```powershell
X_Y --program build/out/modules/Shell/0.1.0/Shell.dll
```

Shell 内：

```
abi
engine create a
engine run a abi
exit
```

插件只负责行编辑/解析；未知命令走：

```
Command::Registry::SetFallback
  -> XyKernelV1::run
  -> local -> shared -> root
```

`help` 仍是 Modules Shell 的内建命令，只列 Shell 内建；其他命令走 kernel。

### deps / libs 语义

- `deps`：X_Y / Modules 体系内、`xy` 能解析的依赖  
  写法：`Name`（自动）、`Name:static`、`Name:shared`  
  `xy` 自动加 include、lib 路径和链接项
- `libs`：作者自己的第三方库、系统库  
  `xy` 只加 `-l`，路径由 `libDirs` 提供
- `libs_windows` / `libs_linux`：仅在对应平台加入的系统库，例如 Windows 文件对话框使用 `libs_windows = comdlg32`
- `includes`：作者自己的第三方头目录，相对插件源码目录
- `libDirs`：作者自己的库目录，相对插件源码目录

Shell 用：

```
deps = Terminal:static, InputConsole:static, Core:static, Memory:shared
libs =
includes =
libDirs =
```

作者自己的库示例：

```
includes = third_party/include
libDirs = third_party/lib
libs = mylib, ws2_32
```

### 构建环境 enve.json

`root` 就是 `out` 目录；`bin` / `modules` / `program` / `lib` / `temp` 默认都相对它：

```json
{
    "root": "build/out",
    "bin": "bin",
    "modules": "modules",
    "program": "programs",
    "lib": "lib",
    "temp": ".cache/tmp",
    "source": "../Modules/Modules"
}
```

- `root`：输出根，相当于 `out/`
- `bin`：X_Y、xy、pkg 等运行文件
- `modules`：模块/包安装目录
- `program`：`xy build` 在没有传 `--out` 时，`kind = executable` 的输出目录
- `lib`：库目录
- `temp`：编译器和链接器的临时目录
- `source`：Modules 源码根

因此上面的配置会将独立程序默认构建到 `build/out/programs/<name>/<version>/<file>`；`xy build ... --out <目录>` 可单次覆盖默认位置。DLL/SO 包的默认输出行为不变。

`xy env` 会打印当前解析结果：

```powershell
PS X_Y> ./build/out/bin/xy env
config  = D:\workbench\X_Y\enve.json
root    = D:\workbench\X_Y\build\out
...
```

`root` 一般相对 `enve.json` 所在目录解析；若配置放在 `out/enve.json`，则 `root` 相对项目根解析，以兼容把配置文件放入输出目录的布局。未写 `root` 时默认是配置文件所在目录；其余目录按解析后的 `root` 解析。
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

- `file` 存在 → 可运行二进制包
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

`install` 面向可直接使用的程序/插件包；开发库的源码、静态库或共享库由 `xy module` 准备。包安装本身仍交给 `pkg`。
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

### xy 可执行程序冒烟测试

测试覆盖 `xy module` 的递归源码/静态/共享准备、模块依赖传递链接，以及独立程序的构建和运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File X_Y\test\xy_build.ps1
```

```bash
bash X_Y/test/xy_build.sh
```

---

## 7. 扩展与 ABI

普通扩展只 include：

```cpp
#include <xy_api.h>
#include <xy_command.h>
```

入口：

| 导出                    | 用途                            |
| ----------------------- | ------------------------------- |
| `xy_abi_version_v1`     | 必须导出，返回 `XY_ABI_VERSION` |
| `xy_program_v1`         | 微程序入口                      |
| `xy_commands_v1`        | 命令表入口                      |
| `xy_module_v1`          | 可选：模块初始化，注册服务      |
| `xy_module_shutdown_v1` | 可选：卸载前反注册服务          |

加载顺序：验 ABI → `xy_module_v1(host)` → `xy_commands_v1(host)` → 登记命令。
卸载顺序：`xy_module_shutdown_v1()` → 摘命令 → 摘服务 → `FreeLibrary`。

规则：

- 跨 DLL 只传 C 类型；不传 `std::string` / 容器 / 异常
- 结构体只尾部追加；用 `size` 判断字段
- `host->alloc` 的指针只能 `host->free`
- 命令表、命令对象归 DLL 所有，卸载前必须有效
- 一次性 program 禁止 `add_service`
- 服务在 `xy_module_v1` 注册，`xy_module_shutdown_v1` 反注册；漏掉的由内核按 owner 清理并报泄漏

### kernel 服务

微程序 / 插件不能直接拿 `Engine.commands`，通过 `XyKernelV1` 把命令交回内核：

```c
#include <xy_kernel.h>

const XyKernelV1* kernel =
    (const XyKernelV1*)host->get_service(host->user, XY_KERNEL_SERVICE);

const char* argv[] = {"help"};
return kernel->run(kernel->user, 1, argv);
```

- 每个 Engine 注册一份 `kernel` 服务
- `run` 查找：本地 commands → container.shared commands → root.commands
- `argv[0]` 是命令名；找不到返回 127
- Shell 微程序只需要解析行，然后调 `kernel->run`
- Modules 侧 `Command::Registry::SetFallback` 可以把未知命令交给 `kernel->run`

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
- `kernel` 服务：每个 Engine 一份 `XyKernelV1`，`run` 走本地→shared→root
- Shell 插件：`Plugins/Shell/0.1.0`，Modules Shell + `kernel` fallback
- Engine 实例化：每个 Engine 一份 host / services / commands；`ModuleRegistry` 也归 Engine 所有（`Start()` 里建）
- EngineContainer：单线程 `engine create/list/destroy/load/unload/run/services/shared/share/unshare/share-command/unshare-command/shared-commands`
- 容器共享服务：`share` / `unshare`，provider 共享期间禁止销毁/卸载
- 命令查找：本地 → container.shared → root（和 services 一致）
- 容器共享命令：`share-command` / `unshare-command` / `shared-commands`
- 容器冒烟测试：`test/container_smoke.ps1` / `container_smoke.sh`
- `xy build`：支持 `kind = executable`，可直接编译并运行独立 C++ 可执行程序
- `xy module`：支持 `auto` / `source` / `static` / `shared`，递归准备模块依赖并生成开发目录
- `xy build`：从 `enve.modules` 解析本地开发模块并递归链接依赖；缺失模块时提示先运行 `xy module`
- `xy` 构建冒烟测试：`test/xy_build.ps1` / `xy_build.sh`
- Memory 共享库：`Memory.dll` / `libMemory.so`，X_Y 不再整库链 Memory
- Memory 依赖包：`out/modules/Memory/{include,bin,lib,package.xy}`
- Memory 显式 API：`X_Y::New/Delete/Malloc/Free/Buffer`；全局 new 不再覆盖
- IO 第三方 C++ 库：`out/modules/IO/{include,bin,lib,package.xy}`，链 IO + Memory
- `pkg` 接受 `kind = library`，list/verify 能识别 Memory/IO
- 跨 engine 服务查找：本地 → container.shared → root（子 Engine）
- 跨 engine 命令查找：本地 → container.shared → root（子 Engine）
- DLL 装载/卸载、微程序包装、命令 owner
- 内建命令、shell、main
- Windows 平台 Library / Process 实测
- Linux 真机验收（用户确认已跑；验收输出未留档）
- `pkg` v1：list / info / verify / install / remove
- `xy`：`build` 本地构建、`module` 准备开发依赖、`install` 安装可运行包；模块清单以 `package.xy` 为主并兼容旧 `module.xy`
- `xy build` 支持 `deps` / `libs` / `includes` / `libDirs`；`deps` 支持 `:static` / `:shared` / `:auto`
- Shell 插件：`Plugins/Shell/0.1.0`，Modules Shell + `kernel` fallback
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
