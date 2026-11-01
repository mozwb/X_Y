# X_Y · Engine 架构与实施计划

亲笔描述：



我想实现一个类似于引擎的东西吧：
引擎包含四部分：
第一部分是构建器：就是支持收集源码并构建产品或者二进制文件
第二部分是包管理工具，构建好的产品可能需要按照版本号进行管理
第三部分是运行器，最内核的一层，最后呈现效果类似于交互式命令:
然后大致可以运行dll内的微程序，可以运行可执行程序(这个跑起来就不用我们管了)，也可以运行一条dll里的指令。能做到这三点。第四部分大概率可行。
第四部分，可以加卸载拓展模块，就是我们当前的内核引擎估计只会提供和构建相关的命令，还有内存管理的命令，还有简单文件管理的命令.
我一直考虑程序成长应当式程序侧还是我们的引擎侧，应为昨天的描述引擎内核一旦确定大概率就是无法变化，变化只有程序侧了。我觉得这样不大好，我们让引擎提供一定能力让程序使用。所以觉得应该拓展运行器得能力，意味着引擎可以加卸载模块以及模块命令，这样子的话，引擎可以根据加载的模块来成长。比如我们X_Y doctor就可以让内核启动后，运行初始化程序再加载进来，当然我们也可以写变量解析器和语法解析器，然后加载进去就有点类似于脚本的成分。这样子的话我们以后要存状态也可以运行命令来调度，就像是我们得让引擎所承载的各个部分能够沟通才行。

这样子主要矛盾大概有：
1.如果两个dll有符号冲突该怎么办，尤其是包版本冲突的不同dll
2.如果我运行的程序类似于挂起一个窗口，那样子的话会不会把程序阻断。如果是可执行程序直接当作额外进程启动它就行，但是对于我们自己的微程序和指令却不能如此做
3.就是有同时运行多个微程序的需求怎么办,（做不到顺序调度）
4.就是模块之间通信到底是运行起来的程序负责还是说我的内核提供


我想的是我们拆开写，先写好一个功能较完善的引擎内核，再做一个内核拓展容器，就是我们的程序只有一个主内核（同时拥有所有权限），然后我们让它运行一个engine容器，这个容器上有多个小engine可以运行不同的指令和微程序，然后我们可以把写操作只让主内核拥有或许还能避免并发，然后这样子就能同时调度多个程序，引擎之间的命令是互通的所以可以支持一定程度的通信，然后如果可以让每个引擎各自有各自的环境还能解决符号链接问题。


  [子程序]      [子程序]   [子程序]   [子程序]
    engine      engine     engine    engine
|__________|_____________|___________|__________|
                    |[容器管理程序]
              engine(main)

> **v2 · 2026-10-21 修订**
> v1（2026-10-20）是"模块化静态库 + 每个项目内嵌源码编译"。
> v2 改为"**engine 宿主 + DLL 插件 + 中心 store**"。第 2 章列出 v1 中被推翻的决策。
> 本文件位于 `_notes/`（不纳入版本控制）。

---

## 现状（2026-10-22 实测，与本文件历史描述不一致处以这里为准）

| 项         | 实测                                                                                                                                                                                                                  |
| ---------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 源码树     | **`X_Y` 是引擎侧唯一源码树**（9 模块：Core / Memory / Timer / Math / Log / Files / DataStore / Decoder / Input），已无 UI / Widget / Render / Media / Network                                                         |
| 工具仓库   | `Modules`（原 `X_YB`）= 自己写的**工具模块源码**（UI/Render/Widget/Media/Network…），与引擎无关，旧历史保留                                                                                                           |
| 版本控制   | `X_Y` 已建 git；`cb80fa0` 清掉 Hazel 模板并接入构建系统                                                                                                                                                               |
| 构建       | 根 `CMakeLists.txt`、`cmake/XYModule.cmake`（lib/dll/exe 三类产物）、`cmake/XYVendor.cmake`（glm/stb/FFmpeg 接线）、`Modules/*/CMakeLists.txt`（平台差异只写在这里）                                                  |
| vendor     | `X_Y/vendor` 仍为空，退回 `../Modules/vendor`（配置期会告警）。自带 vendor 约需 44MB：glm 1.6 + stb 11 + FFmpeg 头 9.5 + 5 个 `.a` 22                                                                                 |
| 真实依赖图 | Log→Core,Timer｜Files→Memory｜Decoder→Core,Memory,Files(+FFmpeg,stb)｜Input→Decoder｜DataStore→Memory,Files；Core/Memory/Timer 无 X_Y 依赖；**Log 与 Math 是 header-only**                                            |
| 已发现的坑 | ① `Decoder/Decoder.h`（公开头）include 了 `<windows.h>`；② include 环 `Decoder.h → Files.h` 而 `Files.cpp → Decoder.h`（`Decoder.h` 里没用 `Files::`，且 `Decode()` 是头内模板，链接图无环）                          |
| 能跑的东西 | **`X_Y.exe`** 四种入口：`--program`（微程序 dll）、`--exec`（外部程序当子进程）、`--run`（单条命令）、无参数（交互 shell）。内核只认动词 `run`；内核命令与 dll 命令在同一张命令表上，模块卸载时自动反注册、泄漏可检出 |
| 模块归属   | 内建（STATIC）：Core / Memory / Timer / Log / Files｜工具侧（将来 dll）：Math / DataStore / Decoder / Input｜未定：内建符号走 host 服务还是链 engine import lib                                                       |

## 0. 一句话

X_Y 是一个**引擎**，不是一个库集合：

- `xy.exe` —— 编译期。找模块、编模块、管产物。
- `X_Y.exe` —— 运行期。装模块、跑程序、管平台。

程序（不是模块）被编译成 DLL；一个 engine 进程跑一个程序。

---

## 1. 四个能力 → 两个进程

| #   | 能力          | 归属     | 一句话                                                |
| --- | ------------- | -------- | ----------------------------------------------------- |
| ①   | 构建器        | `xy`     | 扫 `main.cpp` 声明 → 解析模块闭包 → 调 cmake 编成 DLL |
| ②   | 包 / 版本管理 | `xy`     | 编好的模块进 store，按 ABI key 隔离，别的项目直接复用 |
| ③   | 运行器        | `engine` | 宿主进程，加载程序 DLL，调固定入口，跑主循环          |
| ④   | 模块管理      | `engine` | 模块的装载 / 初始化 / 卸载，含 builtin 与可插拔两类   |

①②是"编译期"问题，③④是"运行期"问题。四者共享同一套模块身份与 ABI key。

---

## 2. 已定决策

| 决策          | 结论                                                                          | 理由                                                     |
| ------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------- |
| 清单位置      | 项目 `src/main.cpp` 里的 `//=== xy ===` 注释块                                | 零额外文件；显式声明优于从 `#include` 反推               |
| 入口符号      | 固定 `xy_program_v1` / `xy_module_v1`，`extern "C"`                           | 文件名只负责**定位**，符号名带版本才能演进               |
| 链接模型      | engine 导出 API（`XY_API`），模块只链 engine 的 import lib                    | 进程内只有一份 XCore / 单例 / RTTI                       |
| 运行时库      | **动态** `libstdc++-6` / `libgcc_s_seh-1` / `libwinpthread-1`，随 engine 分发 | 静态链并不换来自包含，只让每个 DLL 胖 2.2MB（已实测）    |
| engine 文件名 | 固定 `X_Y.exe`，**属于 ABI**                                                  | 模块 import 表写死该名字；改名即 `0xC0000005`（已实测）  |
| 进程模型      | 一个 engine 进程只装一个程序                                                  | 消除版本冲突；多程序并行 = 多进程（日后容器化）          |
| 模块版本      | 进程内唯一版本，冲突即报错                                                    | 同上，因此**不需要版本求解器**                           |
| 基础模块      | `XCore` / `Log` / `DataStore` 静态进 engine，注册为 builtin                   | 唯一实例；其"卸载"是空操作                               |
| 其余模块      | 编成 DLL，可换装                                                              | DLL 边界强制收口内部符号                                 |
| 产物位置      | 中心 store，按 ABI key 隔离                                                   | 跨项目复用，避免重复编译                                 |
| X_Y 仓库      | 纯源码，无 `.dll/.a/.exe`                                                     | 产物只存在于 store                                       |
| 平台代码      | 只在 engine 内                                                                | 模块看不见 OS 头，`XY_PLATFORM_*` 只需 engine 关心       |
| 模块对外表面  | 每模块 1 个 umbrella 头 + `PUBLIC_HEADERS` 清单                               | store 里 `include/` 的版本 diff 就是**公开 API 的 diff** |
| 内部头        | 不作强制搬家                                                                  | "不暴露"由打包清单保证，文件物理位置是次要的             |

### 2.1 被 v2 推翻的 v1 决策

| v1                                             | v2                                              |
| ---------------------------------------------- | ----------------------------------------------- |
| 公共头不搬（避免改 151 处 include）            | 必须收敛到 umbrella；跨模块 include 全部改写    |
| v1 唯一模式 = 项目内嵌源码、X_Y 纯源码无二进制 | 作废。改为 store 中心化产物                     |
| 分发 / SDK 留到 Phase 6                        | 提前成为核心（形态是 store 目录，不是传统 SDK） |
| 构建器只做调度                                 | 增加包管理与产物安装职责（仍不自己实现编译）    |

### 2.2 v1 中保留的部分

Phase 0 仓库清理、Phase 1 模块自述构建、Phase 2 打断 `XCore ↔ Log` 循环并下沉平台代码 —— 全部保留，且 Phase 2 从"清理任务"升级为"平台只在 engine"的架构保证。

---

## 3. 当前实测事实

### 3.1 环境

- MinGW g++ **13.1.0** (`x86_64-posix-seh`)，`D:/QT/Tools/mingw1310_64/bin`
- CMake 4.3.2 + Ninja 4.x
- 基线：`MinGW Makefiles` / Debug，全量构建通过（含 `XYTools.h` 的 `Trim()` 阻断性修复，**该修改尚未提交**）

### 3.2 ABI spike 结论（2026-10-21，代码见 `_notes/spike/abi/`）

| 验证项                                            | 结果     |
| ------------------------------------------------- | -------- |
| exe 导出 C++ 符号 + DLL 链其 import lib           | ✅        |
| `std::string` / `std::vector` 双向传值            | ✅        |
| 跨边界所有权（一边分配、另一边释放）              | ✅        |
| 异常 engine→module、module→engine                 | ✅ 双向   |
| engine 全局单一实例                               | ✅        |
| `typeid` 跨边界一致                               | ✅        |
| `LoadLibrary` + 固定导出名 + `FreeLibrary`        | ✅        |
| 自包含分发（exe + 3 个 runtime，PATH 无 MinGW）   | ✅        |
| Debug / Release                                   | ✅ 两者   |
| 反向对照：两边 `-static-libstdc++ -static-libgcc` | ✅ 也通过 |

**四个可执行的结论**

1. `engine.exe` 的**文件名是 ABI**，改名后启动即崩 → 必须固定为 `X_Y.exe`，`xy doctor` 要检查。
2. 静态链 libstdc++ 不会崩（MinGW 下分配器最终都落到同一个 msvcrt 堆，`typeid` 按名字比较），但**仍依赖 `libwinpthread-1.dll`**，所以"静态"不换来自包含。→ 用动态运行时。
3. `PREFIX ""` 必须显式设，否则产出 `libxy_mod.dll`，破坏"文件名即身份"。
4. 发布集合 = `X_Y.exe` + 3 个 runtime dll + 模块 dll。

**尚未验证**：模块的全局注册表与卸载顺序（spike 里模块无静态注册表，故 `FreeLibrary` 干净）。真实 `Widget` 会往 engine 的表里注册工厂，卸载前必须反注册。留到 B2 验证。

### 3.3 源码现状

| 项             | 数字 / 状态                                                                                                                                      |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| 模块数         | 14（`XCore` `Log` `DataStore` `Middle` `Media` `Model` `Movement` `Network` `Physics` `ProcessMemory` `Render` `Widget` `UI` `GraphicsContext`） |
| 导出宏         | **0**（全是 `STATIC`，无 `dllexport`）                                                                                                           |
| 跨模块 include | 151 处，写法 `#include "XCore/..."`，include 根是 `Modules/`                                                                                     |
| 模块布局       | 模块根放公共头（含子目录），`src/` 放实现                                                                                                        |
| 已知缺失       | `ProcessMemory` 根本没进 CMake，从未被构建                                                                                                       |
| 已知残留       | `dist/`、`CopyFiles.py`、`Unuesd/Png`、被跟踪的 `DataStore/*.log`                                                                                |

---

## 4. 运行期架构

### 4.1 进程与生命周期

```
xy.exe build        ──►  store/<abi-key>/...      （编译期，可离线）
xy.exe run          ──►  spawn X_Y.exe --program programs/HelloXy/1.0/HelloXy.dll

X_Y.exe:
  1. 平台初始化（窗口 / 线程 / 音频后端 / 日志 / 存储根）
  2. 建模块 registry，注册 builtin（XCore / Log / DataStore）
  3. 按程序声明的闭包顺序加载模块 DLL
  4. 调每个模块的 xy_module_v1(host)
  5. LoadLibrary(程序 DLL) → xy_program_v1(host)
  6. 主循环
  7. 程序返回 → 逆序 shutdown 模块 → FreeLibrary
  8. 平台关闭
```

### 4.2 ABI 边界规则

- engine 侧所有对外符号标 `XY_API`（`__declspec(dllexport)`），模块侧自动 `dllimport`
- 模块**只**链 engine 的 import lib；不得自带 XCore
- 模块不得导出 engine 内部类型；对外只有 `XyHost` 与各模块自己的公开头
- 平台头（`windows.h` 等）**只允许出现在 engine 内**
- 模块卸载必须走 `xy_module_shutdown()`，且 engine 在帧边界才真正 `FreeLibrary`

### 4.3 模块身份

一个 DLL 可同时提供两个入口：

| 提供                     | 身份                                  |
| ------------------------ | ------------------------------------- |
| 只有 `xy_program_v1`     | 可运行程序                            |
| 只有 `xy_module_v1`      | 可被装载的模块                        |
| 两者都有                 | 既能被别的程序装载，自己也能跑        |
| builtin（静态进 engine） | 在 registry 里可见，加载/卸载是空操作 |

builtin 与可插拔走**同一条 registry 代码路径**，不设特例分支。

### 4.4 store 布局

```
~/.xy/                                   # 可配置（Debug 产物很大，建议放非系统盘）
├── store/<abi-key>/<Module>/<ver>/
│   ├── include/                         # 只放 PUBLIC_HEADERS，即模块 SDK
│   ├── <Module>.dll
│   └── module.xy.json                   # name / ver / deps / abi / 源 commit
├── store/<abi-key>/engine/
│   └── X_Y.exe + libstdc++-6.dll + libgcc_s_seh-1.dll + libwinpthread-1.dll
└── programs/<name>/<ver>/<name>.dll
```

**ABI key** = 编译器路径 + 版本 + 目标三元组 + 异常模型 + build type + C++ 标准 + `_GLIBCXX_USE_CXX11_ABI` + **engine ABI 版本号**。
最后一项不可省：模块不链自包含，engine 一变整库作废。

---

## 5. Engine 四部分（设计待确认）

> 每一部分都先出设计、与本文件对照确认后再写代码。以下"待确认"是我必须先问你的问题。

### ① 构建器 —— `xy build`

职责：读声明块 → 解析闭包 → 生成 CMake → 编译 → 装 store。

```cpp
//=== xy ==================================
// name    = HelloXy
// modules = Log, Widget, UI
//=========================================
```

**待确认**

1. 声明块的范围：只 `modules`，还是也要 `name` / `defines` / `libs` / 平台例外？
2. "builtin 还是可插拔"由谁决定 —— 模块自己的 `CMakeLists.txt` 声明，还是 `xy` 里一张内置表？
3. 闭包出错（缺模块 / 成环 / 版本冲突）的报错形态。

**最小可用**：`xy build` 能把 HelloXy 编成 `HelloXy.dll`（模块先手工编也接受）。

### ② 包管理 —— `xy store`

职责：模块源码来源、构建产物入 store、ABI key 校验、垃圾回收。

**待确认**

1. **v1 是否需要"下载"？** 你说"模块源码下载并构建好之后"。若 v1 只认本地 `Modules/`，store 就是纯缓存，实现量减一大半；远程仓库可以后加。
2. 版本号从哪来 —— 模块内 `version` 字段、git tag、还是 commit hash？
3. 是否需要 `xy gc`（清理旧 ABI key / 旧版本）。

### ③ 运行器 —— `X_Y`

职责：平台初始化、建 registry、加载程序、主循环、干净退出。

**待确认（最关键的一处）**

1. **谁驱动主循环？** engine 驱动（engine 每帧调 program 的 `tick`），还是 program 自己跑循环（engine 只提供窗口/日志等服务）？
   - 前者：engine 掌握时序，模块能插进帧流程，热换装更容易，但引擎"侵入"更深。
   - 后者：程序自由度高，engine 更像一个启动器，但帧相关的服务难以统一。
2. `XyHost` 暴露到什么程度 —— 是"服务定位器"（`host->Get<ILog>()`），还是直接暴露 Engine 对象？
3. 程序如何拿到命令行参数、如何返回退出码。

### ④ 模块管理 —— `engine` registry

职责：模块生命周期、依赖拓扑、装载顺序、卸载。

**待确认（第二关键）**

1. **模块之间怎么互相引用？**
   - (a) 走 host 服务表：`Widget.dll` 只认 `IModuleHost` 接口，不链 `UI.dll` → 任何模块都能独立卸载，但要写一层接口。
   - (b) 直接链符号：`Widget.dll` 链 `UI.dll` 的 import lib → 简单，但卸载变成"整组卸载"。
   这一条决定了模块的粒度，也决定了 ① 里"可插拔"能到什么程度。
2. 卸载粒度与时机（帧边界；哪些模块允许 unload）。
3. builtin 模块是否也能被"重启"（例如 `Log` 换配置）。

---

## 6. 阶段与顺序

### A. 模块层清理（先做，小步）

- **A1 仓库卫生**：`git rm --cached` 运行日志；删 `dist/`、`CopyFiles.py`、`Unuesd/Png`、`samples/build`、`test/build`
- **A2 模块自述构建**：`cmake/XYModule.cmake`（GLOB + 平台目录筛选 + `SHARED`/`STATIC` + `PUBLIC_HEADERS`）；每模块 2~4 行 `CMakeLists.txt`；打断 `XCore ↔ Log`；平台链接库条件化；`ProcessMemory` 纳入（仅 Windows）

**验收**：全量构建 + ctest 与基线一致；`-DXY_MODULES=Log` 的构建日志里不出现 FFmpeg / imgui / freetype。

### B. engine 四部分（每步先设计确认）

- **B1 运行器骨架**：手工编一个 `HelloXy.dll`，`X_Y.exe` 能加载并跑起来
- **B2 模块管理**：registry + 生命周期 + 卸载反注册验证
- **B3 构建器**：`xy build` 自动产出模块与程序
- **B4 包管理**：store 落盘、ABI key、跨项目复用

> 顺序建议与你列的 ①②③④ 不同：**先让 engine 能跑（模块可以手工编），再让 `xy` 自动产出**。理由是每一步都能当场验证，不会出现"两边都没跑通"的死角。是否采用，等你定。

### C. 迁移真实项目

- `Project/_legacy/` 收纳三个旧 exe（保留不删）
- `Project/HelloXy`：最小验证（`Log` + `Widget`）
- `Project/VisualTest`：`modules = ["UI"]`，端到端验证

### D. 模块表面收敛

- 每模块一个 umbrella 头；`PUBLIC_HEADERS` 清单
- 151 处跨模块 include 改为只 include umbrella（脚本 + 编译验证）
- 内部头搬 `src/` 作为可选清理

### E. 文档收尾

- 重写 `README.md`（现在的结构描述是错的）
- 模块作者指南：加模块 = 建目录 + 2~4 行 CMakeLists + 声明 DEPS
- DEVLOG 记录本次重构

---

## 7. 风险

| 风险                           | 对策                                                                    |
| ------------------------------ | ----------------------------------------------------------------------- |
| 模块全局注册表在卸载后悬垂     | 强制 `xy_module_shutdown()` 反注册；帧边界才 `FreeLibrary`；B2 专项验证 |
| ABI key 漏项导致复用不兼容产物 | key 必须含 engine ABI 版本号；`xy doctor` 打印当前 key                  |
| 每个项目首次构建要重编闭包     | 闭包只含声明模块；store 跨项目复用；`xy gc` 回收                        |
| 工具链升级导致 store 整体失效  | 这是设计意图（engine 即运行时）；`xy` 给出明确提示而非静默失败          |
| 头文件收敛改动面大             | 脚本一次性替换 + 全量构建验证；不做物理搬家的强制要求                   |
| 构建器拖慢主进度               | 先手写 DLL 验证运行期，再让 `xy` 接管编译期                             |
| 中文源码编码                   | 保持 `-finput-charset=UTF-8 -fexec-charset=UTF-8` 不变                  |

---

## 8. 附录：ABI spike 复现

```powershell
cd _notes\spike\abi
$env:PATH = "D:\QT\Tools\mingw1310_64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER="D:/QT/Tools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="D:/QT/Tools/Ninja/ninja.exe"
cmake --build build
cd build\out
Copy-Item D:\QT\Tools\mingw1310_64\bin\lib{gcc_s_seh-1,stdc++-6,winpthread-1}.dll .
$env:PATH = "C:\Windows\system32;C:\Windows"
.\X_Y.exe
```

关键 CMake 三行：

```cmake
set_target_properties(X_Y PROPERTIES ENABLE_EXPORTS TRUE)
target_link_libraries(xy_module PRIVATE X_Y)
set_target_properties(xy_module PROPERTIES PREFIX "")
```

---

## 9. 进度（完成即更新）

### 2026-10-22 · 构建系统

- [x] 根 `CMakeLists.txt`：C++20 / UTF-8 全链路 / `XY_PLATFORM_*` / `XY_DEBUG|XY_RELEASE`
- [x] `cmake/XYModule.cmake`：一个目录=一个模块，`HEADER_ONLY`、`DEPS`、`EXCLUDE`；自动算出 `XY_PUBLIC_HEADERS`（给将来 store 打 `include/` 用）
- [x] 9 个模块全部接进构建；Log/Math 判为 header-only
- [x] vendor 自动探测（`vendor/` → `../X_YB/vendor`），`-DXY_VENDOR_DIR=` 可覆盖；FFmpeg 用预编译 `.a`
- [x] `-DXY_MODULES=Log` 选择器（依赖闭包，实测算出 `Log;Core;Timer`）
- [ ] 全量 `cmake --build` 通过（待砚台执行）

### 2026-10-22 · 三类产物 + 平台接线下方到模块

- [x] `xy_module(<Name> [HEADER_ONLY|STATIC|SHARED] [DEPS ...] [EXCLUDE ...])` —— lib / dll 一种函数
- [x] `xy_program(<Name> [DIR ...] [DEPS ...] [EXPORTS])` —— exe；`EXPORTS` 给 engine 用（exe 导出符号供 dll 反向链接）
- [x] `XY_MODULE_LINKAGE=STATIC|SHARED` 全局默认；单个模块可覆盖。`Memory` 写死 STATIC（全局 new/delete 只能有一份）
- [x] dll 严格导出：MinGW 加 `-Wl,--exclude-all-symbols`，Linux 加 `-fvisibility=hidden`（`XY_STRICT_EXPORTS`，默认 ON）
- [x] dll 静态链静态库时配置期告警（“全局状态会在每个 dll 里各一份”）
- [x] 平台差异全部下放：根不再有 `if(WIN32) target_link_libraries(...)`；每个 `Modules/<Name>/CMakeLists.txt` 自己写 `if(WIN32) … return()`、自己的 `comdlg32`/`ole32`/`bcrypt`
- [x] `cmake/XYVendor.cmake`：`xy_vendor_glm` / `xy_vendor_stb` / `xy_vendor_ffmpeg`，模块只链目标名
- [x] 非 Windows 平台：Decoder（公开头带 `<windows.h>`）、Files、Input、DataStore 链式 `return()` 跳过并打印原因，不留假 target

**已验证（configure 级）**：STATIC 出 `out/lib/lib*.a`；`-DXY_MODULE_LINKAGE=SHARED` 出 `out/bin/Core.dll` + `out/lib/libCore.dll.a` 且 `Memory` 仍为 `.a`；另建临时工程验证 `xy_program` 出 `out/bin/smoke_app.exe` 并正确链接 `libCore.dll.a`。

### 2026-10-22 · B1 最小运行器 —— **已跑通**

- [x] `Engine/include/xy_api.h`：唯一 ABI 契约（C 头，C/C++ 共用）。只传 C 类型、内存成对、结构体只尾部追加字段、`abi_version` 不符即拒载
- [x] `Engine/src/Host.cpp`：`log` / `alloc` / `free`；分配器只留一条路径（Win 全 `_aligned_malloc`，POSIX 全 `posix_memalign`），`HostFree` 无需知道 `align`
- [x] `Engine/src/main.cpp`：`--program <dll> [args...]`；退出码 64 用法 / 65 载入失败 / 66 缺入口
- [x] `Engine/CMakeLists.txt`：`xy_program(X_Y EXPORTS)`（`ENABLE_EXPORTS`，为将来模块反向链 exe 的 import lib 准备）
- [x] `Programs/HelloXy/`：最小程序 DLL，只认 `xy_api.h`，不链引擎
- [x] `xy_module(... DIR <dir>)`：`Programs/<Name>` 复用同一套 DLL 规则
- [x] **严格导出实测**：`HelloXy.dll` 导出表只有 `xy_program_v1` 一个符号
- [x] 运行时实测：argv 透传、`host->alloc/free` 成对、退出码正确

**下一步**：B2 模块装载 —— 用 `Modules/Timer` 这种无依赖模块验 `xy_module_v1` 装载 / 卸载，重点看卸载后静态注册表是否悬垂。

### 2026-10-22 · B2 模块装载 —— **已跑通**

- [x] `xy_api.h` 追加服务表（`add_service` / `get_service` / `remove_service`）+ `XY_HOST_HAS` 宏；`size` 40 → 64，老模块不受影响
- [x] `XY_MODULE_SHUTDOWN_ENTRY`：模块卸载前的反注册入口（可选）
- [x] `Engine/src/Services.*`：名字 → 指针，带 owner；归属由引擎在调用期间标注，模块无法伪装
- [x] `Engine/src/ModuleRegistry.*`：装载 → 初始化 → 逆序卸载；卸载时反查泄漏并强制移除
- [x] `X_Y --module <dll>`（可重复），程序 DLL 先于模块卸载
- [x] 夹具 `test/probe`（守规矩）与 `test/leaky`（故意不反注册）
- [x] 实测：probe 干净退出；leaky 被引擎抓出 `泄漏 Leaky 注册的服务 'leaky.marker'`
- [x] vendor 改为「只探测不 FATAL」；引擎内核不依赖 vendor
- [x] 模块定型：内建写死 STATIC；工具侧暂留 lib
- [ ] **待决**：内建符号走 host 服务表（a）还是链 engine import lib（b）—— 决定工具侧模块能不能切 SHARED


