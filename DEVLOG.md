# DEVLOG
## 只保留最近十次改动多了就删除


## 2026-09-17 窗口生命周期三连：Panel 组件 double-free 修复 + 主窗退出语义 + Application 托管/退出清理

> 起因：砚台发来 Qt Creator 调试器截图，崩在 `std::default_delete<X_Y::Component>::operator()`；紧接着又发现：关主窗后其他窗口泄漏、关掉所有窗口程序也不退出。

### ① Panel 子类组件 double-free（崩溃根因）

- **根因**：`LogViewer` / `HexViewer` 把组件**同时**挂到两处 owns ——
  自己的 `unique_ptr` 成员 **和** `Panel::AddComponent(comp)`（内部
  `m_Components.emplace_back()` 接管所有权）。
- **析构顺序致命**（派生成员 → 基类）：`~LogViewer` body 跑完 → 成员
  unique_ptr **先 delete 一次** → 基类 `~Panel()` 再对**同一（已释放）指针**
  delete 一次 → 读已失效 vptr → 崩。`Component` 有虚析构也没用，问题不在虚析构。
- `Panel.h` 头注释本就声明这些应是"借用指针"（还点名了 LogViewer 的 TagStrip），
  unique_ptr 是写歪了。
- **修法**：Panel 拥有组件，子类成员改**裸借用指针**，构造改 `new` +
  `AddComponent(ptr)`（所有权转交 Panel）。
  - `LogViewer.h/.cpp`：`m_TagStrip`/`m_KeywordInput`/`m_ScrollArea` → 裸指针；
    `m_LogStripe` **保持 unique_ptr**（不进 Panel 树，仅被 ScrollArea 借用）。
  - `HexViewer.h/.cpp`：`m_FileBar`/`m_ScrollArea` → 裸指针；`m_Content` **保持 unique_ptr**（同理）。
  - （`Horizontal`/`Vertical` 是纯借用容器，不 owns、析构不 delete，无问题。）

### ② 关窗不退出：FirstWin → 手动主窗

- 旧 `Application::FirstWin` 语义是"进程里第一个建的顶层窗口 = 负责退出的窗口"。
  `XWidget` 构造时 `!IsFirstWin()` → `updateFirstWin()` + `connect(WindowClose, app, &appClose)`。
- 但自检探针窗（`test/src/main.cpp` 的 ProbeWindow）**先建、抢走 FirstWin**；
  它 `destroy()` 走 `DestroyWindow()`（**不经 WM_CLOSE**）→ 不触发 appClose，
  `FirstWin` 也不复位 → 三个真窗口全走"只关自己"分支 → 没人退程序。
- **改为手动指定主窗**：删 `FirstWin`/`IsFirstWin`/`updateFirstWin`；
  加 `SetMainWindow(XWidget*)` / `GetMainWindow()` / `OnMainWindowDestroyed()`。
  `XWidget` 顶层窗口一律 `connect(WindowClose, this, &XWidget::destroy)`。
  `SetMainWindow` 负责把主窗的 WindowClose 接到 `appClose`。

### ③ Application 托管名单 + 显式退出清理（Shutdown）

- **问题**：关主窗 → `Running=false` → 主循环一退，其他窗口**从没 destroy**
  → 不进延迟回收 → 析构不跑、`LogViewer` 的 Ticker 线程不停（泄漏）。
- **关键约束**：退出清理**不能直接 `delete`** —— `~BaseWin`→`Destroy()`→
  `DestroyWindow()` 会**同步发 WM_DESTROY**，又入队一个 `WindowDestroy(this)`；
  直接 delete 会让该事件指向已释放内存 → UAF。正路是 `w->destroy()` + **泵消息**。
- **新增**：`Own(XWidget*)`（幂等托管）/ `ForgetOwnedWindow(XWidget*)` /
  `Shutdown()`（逐个 destroy + `pushEvents/ProcessEvents` 直到名单空 + 兜底 flush）。
  `XWidget::OnNativeDestroyed` 开头脱名单。
- **语义**：关主窗 = 退出；`Shutdown()` 在主循环结束后**显式调用**，清掉剩下所有托管窗口。
- **无二次释放**：`destroy()` 幂等 / `delete` 只来自 `OnNativeDestroyed` 的
  `DeferRecycle`（`m_RecycleQueued` 门闩）/ `Own` 幂等 + 退场即脱名单。

### 涉及文件

`Modules/Widget/Application.h/.cpp`、`Modules/Widget/src/XWidget.cpp`、
`Modules/UI/Panel/LogViewer.h`、`Modules/UI/src/LogViewer.cpp`、
`Modules/UI/Panel/HexViewer.h`、`Modules/UI/src/HexViewer.cpp`、
`Modules/UI/src/tabhostcontainer.cpp`、`test/src/main.cpp`

### 待办

- 浮动面板窗已在 `tabhostcontainer.cpp` 补 `Application::instance()->Own(window)`（砚台补）。
- 泛化托管接口（`void*` + cleanup action）**暂缓**，以后再看。
- 编译验证交砚台。


## 2026-09-16 内存模块③-e：补 X_Y::New / X_Y::Delete（命名对称）

> 砚台："为啥不是 `X_Y::` 这样子使用，你把他封装到 `X_Y::new` 和 `X_Y::delete` 里好了"
> 拍板：统一成命名空间级；`Memory::Instance().Allocate<T>()` / `Deallocate` **保留**。

### 改了什么

路 B 之前只有 `Malloc`/`Free` 是命名空间级的，对象级却要写
`Memory::Instance().Allocate<T>(...)` —— 又长又和 `new/delete` 不对称。补上：

```cpp
auto* o = X_Y::New<Widget>(args...);   // 分配 + 构造，走策略   ★新
X_Y::Delete(o);                        // 析构 + 归还          ★新

auto* s = X_Y::NewFrom<BackendType::Slab, Widget>(args...);
X_Y::DeleteFrom(s);
```

**完整对称表（现在）：**

| 裸内存                            | 对象                                 |
| --------------------------------- | ------------------------------------ |
| `X_Y::Malloc(n)` — 走策略         | `X_Y::New<T>(...)` — 走策略          |
| `X_Y::AllocFrom(B, n)` — 指定后端 | `X_Y::NewFrom<B, T>(...)` — 指定后端 |
| `X_Y::Free(p)`                    | `X_Y::Delete(p)`                     |
| `X_Y::MallocFrom(B, n)`（别名）   | `X_Y::DeleteFrom(p)`                 |

### ⚠️ 实现上的一个坑（写的时候发现的，已避开）

`Memory::Allocate<T>` 有**两个重载**：

```cpp
T *Allocate(BackendType type, Args&&...args);
T *Allocate(Args&&...args);
```

**若 `T` 的某个构造参数恰好是 `BackendType`**，重载解析会选前者，
把用户的构造参数当成"指定后端"，**静默换后端、还少传一个构造参数**。

所以 `X_Y::New<T>` **没有**去转发 `Allocate`，而是直接展开成
`allocate(sizeof(T), Default)` + placement new —— 无歧义。
`Allocate` 那两个重载**保留**（既有调用点在用），但已在注释里标出这个坑，
并注明新代码优先用 `X_Y::New` / `X_Y::NewFrom`。

### 成对约定（更新后的完整版）

```
new 发的                        → delete
X_Y::New / NewFrom 发的         → X_Y::Delete / DeleteFrom
X_Y::Malloc / AllocFrom 发的    → X_Y::Free
```

混用 = 把内存还给错误的堆 → 崩。**不做检测、不做兜底。**

> ⚠️ 特别提醒：`X_Y::Malloc(n)` 分配的是 `malloc(n + 32)`，
> 返回的是 `raw + 32`。所以误用 `delete p` 会让 `free()` 拿到块中间的
> 地址 → **必崩**（不是"歪打正着对上了"）。

### 涉及文件

`Memory/XMemFacade.h`：加 `X_Y::New` / `X_Y::Delete`；更新文件头两条路说明、
用法示例、成对约定表；给 `Memory::Allocate` 标注重载歧义坑。


## 2026-09-16 内存模块③-d：全局 new/delete 退回"只统计"（跨 DLL 崩溃修复）

> 起因：跑 WindowRuntime 自检，第一条 `XDEBUG` 就崩。栈是
> ```
> msvcrt.dll!free()
> libstdc++-6.dll!std::filesystem::path 析构
> X_Y::XPath::operator/          FilesSystem.h:152
> X_Y::DataStore::IndexPath      DataStore.cpp:48
> X_Y::DataStore::LoadIndex      DataStore.cpp:79
> ```
> 实测：`main` 开头加 `Memory::Instance().setEnabled(false)` 后**不崩**。
> 砚台拍板：**把路堵死** —— new/delete 只统计，完整能力另走显式 API。

### 一、根因（不是某行代码写错，是路本身走不通）

原设计让全局 `new` 走门面 `allocate`（加分配头、按策略选后端），
目标是"所有分配没人能绕过"。**在 Windows + MinGW 下这个假设不成立**：

- EXE 与各 DLL **各自绑定符号**，而 MinGW 的 `libstdc++-6.dll`
  是**独立的一个运行时**；
- 于是会出现"内存由 libstdc++ 的堆分配、却经 EXE 的 `operator delete` 释放"；
- 门面读 `ptr-32` 找不到分配头 → 兜底 `std::free(ptr)`；
- 而那个 `std::free` 解析到 **`msvcrt.dll!free`** —— 不是同一个堆 → **崩**。

**★ 关键认识：重载全局 `operator new/delete` 隐含"全进程只有一个堆"的假设。
只要存在第二个运行时/DLL 边界，假设就破。** 砚台将来还要写自己的 DLL，
所以这条路必须放弃，不能靠"静态链 libstdc++"拖时间。

### 二、修法：职责彻底分开（两条路）

|          | 路 A：全局 new/delete          | 路 B：显式分配器                                     |
| -------- | ------------------------------ | ---------------------------------------------------- |
| 入口     | `new` / `delete`               | `Malloc`/`Free`、`AllocFrom`、`NewFrom`/`DeleteFrom` |
| 内存来源 | `::malloc` / `::free`          | 门面 `allocate` → 后端（Crt/Slab）                   |
| 分配头   | ❌ 无                           | ✅ 有（记 size + backend + user + checksum）          |
| 策略     | ❌ 不参与                       | ✅ 参与（`Default` 时问策略）                         |
| 统计     | ✅ 记次数与字节                 | ✅ 完整记（含后端分布、overhead）                     |
| 跨 DLL   | ✅ 安全（malloc/free 同堆配对） | ✅ 安全（只碰自己发的指针）                           |

**全局 `new` 现在长这样**（`XMemGlobalNew.cpp` 整个重写）：

```cpp
void *operator new(std::size_t size)
{
    void *p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    NotifyAlloc(size);          // ← 只记账，不碰后端/header/策略
    return p;
}
void operator delete(void *ptr, std::size_t size) noexcept
{
    NotifyFree(size);           // ← sized delete：字节数精确
    std::free(ptr);             // ← 无条件原生 free
}
```

**门面新增两个纯记账入口**（`XMemFacade.h/.cpp`）：

```cpp
void notifyAlloc(uint64_t size);   // → m_Counter.onAllocate(size, BackendType::Crt)
void notifyFree(uint64_t size);    // → m_Counter.onDeallocate(size, BackendType::Crt)
```

- 只动统计数字，**绝不碰内存/后端/header** → 自举安全；
- 固定记到 `Crt` 槽（new 走的就是标准库堆）；
- `notifyAlloc` **不记 `onOverhead`** —— new 这条路没有分配头，
  不能把那 32 字节算进去（以前会算，是错的）。

> ⚠️ sized delete 的字节数**不保证一定精确**：编译器在大小可知时才会调
> `operator delete(void*, size_t)`。所以不带 size 的版本必须保留，
> 它传 0 = "只知道次数，不知道字节"。

### 三、⚠️ 成对约定（硬规矩，用错就崩，不救）

```
new 发的                      → 只能 delete
Malloc/Alloc/AllocFrom/NewFrom 发的 → 只能用 Free/DeleteFrom
```

两边混用 = 把内存还给错误的堆。**不做检测、不做兜底**（砚台："把路堵死就行"）。

> 因此 `Memory::deallocate` 里"判不过就 `std::free`"那段**保留但降级为兜底**：
> 它只服务 `Free`/`Deallocate` 手滑传错指针的情况，不是给混用开的口子。

### 四、涉及文件

| 文件                           | 动作                                                                                                 |
| ------------------------------ | ---------------------------------------------------------------------------------------------------- |
| `src/Memory/XMemGlobalNew.cpp` | **整个重写**：new→malloc+记账，delete→free+记账；删掉所有 `allocate`/`deallocate` 调用               |
| `Memory/XMemFacade.h`          | 加 `notifyAlloc`/`notifyFree`；通道说明改写成"两条路"；用法示例重写；`setEnabled` 说明改成"排查开关" |
| `src/Memory/XMemFacade.cpp`    | 实现两个记账函数；`deallocate` 注释改为"只服务显式分配器"                                            |

**其它模块零改动** —— `Buffer` 用的 `Memory::Alloc/Free` 仍走完整门面（路 B）。

### 五、连带影响（要知道）

- **STL / `std::string` / `std::filesystem::path` 等不再进策略、不进 Slab**，
  只被统计到次数与字节。这是路 A 的必然代价，也正是换来跨 DLL 安全的代价。
- **"按后端分布"的数字含义变了**：`Crt` 栏现在 = "普通 new 的量"，
  `Slab` 栏只反映**显式调用**的量。数字反而更诚实。
- 将来写 DLL：DLL 内部的 new/delete 用自己的运行时，**根本不进本门面**，
  互不干扰 —— 这正是要的效果。

### 六、验证（交给砚台）

- 自检重跑，第一条 `XDEBUG` 不再崩（原崩溃点：DataStore::LoadIndex → IndexPath）；
- `main` 开头那行 `setEnabled(false)` 可以**留着**——它是排查这类问题的第一手段；
- `stats()` 里 `Crt` 的次数/字节应当有数（所有 new 都被统计）；
- `MallocFrom(Slab, n)` 后 `Slab` 栏有数，`Free` 后 `liveBlocks()` 归零；
- ⚠️ 检查现有代码**有没有把 `NewFrom`/`Malloc` 的返回值用 `delete` 释放**的
  —— 按新约定那是错的（`Buffer` 用的是 `Alloc`/`Free`，成对，没问题）。


## 2026-09-14 内存模块③-c：策略改裸函数指针 + 删掉释放路径上的后端遍历

> 承上条（③-b SlabBackend 接入）。砚台看过代码后提了两点：
> ① "我们先处理你说的策略是 lambda 表达式的问题较好"；
> ② "删除就是门面更新在对应后端更新就行" —— 质疑释放为什么要遍历后端。
> 拍板：策略用函数指针；`own()` 留作自检；新后端挂载姿势等真要加时再说。

### 一、策略：`std::function` → 裸函数指针

```cpp
using AllocPolicy = BackendType (*)(uint64_t);   // 以前是 std::function<...>
```

**为什么必须改**（不是"更规范"，是有真实的递归路径）：

```
用户 setPolicy(捕获物较大的 lambda)
  → std::function 构造：捕获物塞不进 SBO 小缓冲
  → 它调 operator new —— 而本模块重载了全局 operator new
  → Memory::allocate() → 此刻 m_Policy 还没就绪 → 自举递归 / UB
```

- 旧代码"手动 malloc + placement new 放 `std::function`"**解决不了**这个问题：
  那只安排了 `std::function` 对象本身在哪，**捕获物照样在堆上**。
- 改为裸指针后：`setPolicy` 只是一次原子 store，`clearPolicy` 存 nullptr，
  `hasPolicy` 判非空。零分配、零析构，门面成员彻底不用运行期构造。
- **附带好处：把错误挡在编译期** —— 捕获 lambda 直接不匹配这个类型，编译不过。
  需要"策略读配置"时，把配置做成门面成员 + 写无捕获静态函数去读，
  而不是让策略去捕获。

> ⚠️ 我上一轮把严重性讲夸大了一次，此处更正：**空捕获 lambda（如文档里
> `[](uint64_t n){ return n<=512 ? Slab : Crt; }`）其实不会炸**（它存成函数指针）。
> 会炸的是捕获物超过 SBO 的。所以这条是"看捕获大小"的隐患、不是必炸，
> 但**不能把可用性押在编译器 SBO 大小这种不受控条件上**，故照样改掉。

### 二、释放路径：删掉"遍历后端问 own()"这一层

`deallocate` 现在是**纯 O(1)**：

```cpp
void* raw = ptr - kHeaderSize;      // header 就在就前面
if (!HeaderValid(hdr, ptr))  { std::free(ptr); return; }   // magic + user + checksum
if (hdr->magic != kAllocMagic) { /* 重复释放，告警并 return */ }
// 按 header 里的 backend 字段找后端 → 归还
```

**删掉的那层是净负债**，理由是砚台点出来的那句"删除就是门面更新+对应后端更新"：
中间那层 `ownsFast()`（遍历所有后端、问"这地址是不是你的地盘"）
**没防住任何东西** —— 紧跟着的三连验真照样要读同一块内存，
所以它没换来任何安全性，却：

- 让**每一次 `delete`**（含所有纯 CRT 指针）变成 O(后端数)；
- Slab 的 `own()` 要抢 `m_IndexLock` + 二分 → **全局串行点**；
- 把判决拆成三段，段与段之间留了空隙（我上一轮还得专门补个 magic 检查来堵）。

> 关键认识：**只要 Slab 被注册进 `m_Backends` 一次（哪怕只 `MallocFrom(Slab,n)` 一次），
> 之后所有 `delete` 都要走 Slab 的 `own()`** —— CRT 路径被 slab 连累。

### 三、`own()` 保留作自检

删掉的是"释放热路径上的遍历"，不是 `own()` 接口本身。新增一个明确的自检入口：

```cpp
IMemoryBackend* Memory::backendOwning(void* ptr);   // 只问后端区域表，不读 header
```

- `owns(ptr)` —— 读 header 三连验真（与 deallocate 同判据）；
- `backendOwning(ptr)` —— 只问后端自己的区域表（Slab 的 chunk 二分）。
- ⚠️ `ownsFast()` 已**删除**，不要再加回来。
- 两者都不在分配/释放路径上，所以 `own()` 可以"精确但偏慢"。

### 四、涉及文件

| 文件                        | 动作                                                                                                                                |
| --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `Memory/XMemFacade.h`       | `AllocPolicy` 改裸函数指针；`m_Policy` 改 `atomic<AllocPolicy>`；删 `ownsFast` 声明、加 `backendOwning`；用法示例改成无捕获静态函数 |
| `src/Memory/XMemFacade.cpp` | `ResolveWithSize` 适配函数指针；`deallocate` 删遍历、改纯 header 判决；`ownsFast` 删除；`owns` 改纯 header；新增 `backendOwning`    |

> `XMemBackend.h/.cpp` 未改 —— `own()` 本就在接口里，只是不再被释放路径调用。
> 另：旧 `Modules/XCore/src/Memory/XMemory.cpp` 与 `Memory/XMemory.h` 已由砚台删除，
> 之前的"两个 `X_Y::Memory` 抢定义"隐患解除。

### 五、验证（交给砚台）

- 全局 `new`/`delete` 照常（含 STL）；
- `setPolicy(&无捕获函数)` 生效；尝试传捕获 lambda **应当编译不过**；
- **释放不再抢 Slab 的索引锁**：`MallocFrom(Slab, n)` 之后，
  普通 `new`/`delete` 的路径应与未启用 Slab 时一致（可对比 profile）；
- `owns()` / `backendOwning()` 自检：门面指针 true/Slab，栈指针 false/nullptr。


## 2026-09-14 内存模块③-b：SlabBackend 接入 + 退役 OwnedSet（header 双字段验真）

> 砚台："Memory 模块有一个内存后端还没有接入你先接入一下，另外你使用什么哈希表来存什么
> 是不是经过我们的门面分配内存，但是我觉得你直接在那个 header 里多加两个验证字段就行，
> 没必要还要额外存储。"
> 拍板：① 删掉 OwnedSet；② Slab 仅显式调用（默认仍全走 Crt）；③ 保留 slab 分配时的清零。

### 一、接入了什么

`BackendType::Slab` 此前只是枚举里的一个名字 —— `backendFor()` 返回 nullptr，
`allocate()` 在 `if (!backend) return nullptr;` 那行**静默失败**。
现在把旧 `Memory/XMemory.h` 的 slab 实现搬进新后端体系（`SlabBackend`）。

**不是照抄**，四处必须改（旧实现放不进新体系）：

| #   | 旧 `XMemory.cpp`                               | 新 `XMemBackend.cpp`                | 为什么                                                                              |
| --- | ---------------------------------------------- | ----------------------------------- | ----------------------------------------------------------------------------------- |
| 1   | `std::malloc` 取 chunk                         | `::malloc`                          | 后端铁律：只用 `::malloc/::free`                                                    |
| 2   | `std::vector`/`std::mutex`/`std::shared_mutex` | `::malloc` 手写数组 + 自旋锁        | 构造期 `new` → 门面 → 后端 → 自己 → **自举递归**。slab 后端一旦注册成静态对象就必炸 |
| 3   | 释放靠 `FindChunkByAddr` 二分                  | 同一套区间查询，但对外叫 `own(ptr)` | 门面需要它做 fast-path（见下）                                                      |
| 4   | 后端内 `memset(ptr,0,size)`                    | 保留（砚台定）                      | 与旧的逐位行为一致，便于对比新旧输出                                                |

档位/chunk 沿用旧值：**8 档 64B…64KB，chunk 64KB**；
chunk 回收沿用旧不变量：**只有该 chunk 自己全部空闲才 free**
（旧代码特意注释过：不能用 bin 的 freeCount 判断单个 chunk）。

### 二、OwnedSet 退役 → header 加两个字段

`AllocHeader` 16B → **32B**（仍对齐 16）：

```cpp
struct AllocHeader
{
    uint64_t  size;      // 用户请求字节数
    uint64_t  checksum;  // ★新：由 size+backend+user 算出的自校验
    uintptr_t user;      // ★新：用户区地址（= raw + kHeaderSize）
    uint32_t  magic;
    uint8_t   backend;
    uint8_t   pad[3];
};
```

**为什么这下真的够了**（砚台判断的依据）：

1. 归属判定本来就靠 header（`magic`+`backend`），表只被用来"先查表再读 header"；
2. **旧 `XMemory::Free` 早就在证明这点** —— 它直接 `FindChunkByAddr` 判归属，**压根没有表**；
3. 表是 per-pointer 开销：每次分配插一次、释放删一次、扩容还要重哈希；
4. 两个新字段防的正是表**防不住**的那类错：重复释放、指针被改、跨后端错配。

验真三连（**不查任何表、不解引用外部指针**）：

```
magic == kAllocMagic && user == ptr && checksum == Checksum(*hdr)
```

`deallocate` 的实际判决顺序（**三段，别简化**）：

```
① 快路径：地址落在自家区域内（问各后端 own()）→ 读 header 不可能段错误
② 慢路径：读 header 做三连验真
   ①②都不过 → 不是门面发的 → ::free 交回标准库
③ 到这儿说明"确实是我发的"，但还要再看一眼 magic：
   若 magic 已不是 kAllocMagic → 是【重复释放】（归还时会把 magic 擦成 0）
   → 告警并 return，【不再归还】，避免把同一块还两次
```

> ⚠️ ③ 是写完后补的，别删。原因：slab 的 block 还回 free list 后，
> 它所在的 chunk 仍是自家页 → `ownsFast` 照样 true。少了 ③ 的话，
> 第二次 `delete` 会把已空闲的 block 再还一次 free list →
> **同一地址在表里出现两次** → 后续两次分配拿到同一块内存。
> 这种 bug 比直接崩溃难查得多（典型的 double-free 变异）。

> 顺带把一直空着的 `IMemoryBackend::own()` 用起来了。

### 二·补、实现中发现的锁顺序问题（ABBA 死锁，已处理）

写到一半自查发现一个**低负载永远不炸、一上并发就卡死**的坑：

- `deallocate` 的顺序是"先拿 `m_IndexLock` 查区间 → 放掉 → 再拿 bin 锁"；
- 而我第一版 `AddChunk` / `DropChunk` 是"**持 bin 锁时**去动区间表"。

两种顺序并存 = 经典 ABBA 死锁。**已按"持 bin 锁时绝不拿 m_IndexLock"重排**：

| 场景         | 做法                                                                                                         |
| ------------ | ------------------------------------------------------------------------------------------------------------ |
| 扩了新 chunk | `AllocFromBin` 把新 chunk 的 base 回报出来，调用方**放掉 bin 锁之后**才 `IndexChunk()`                       |
| 回收空 chunk | bin 锁内只 `DetachChunkNoFree()`（摘数组+记账，**不 free**）→ 放锁 → `UnindexChunk()` → **最后才** `RawFree` |

**"先摘索引、再 free"这条顺序是必须的**：否则会出现"`own()` 还认这块地、
但内存已经还给 OS"的窗口，门面就会在那一刻去读已经不属于自己的 header
（→ 读野内存 + 误判归属）。

回收空 chunk 时还有个小竞态：放锁 → 重新拿锁之间，别的线程可能已经把该 chunk
收掉了。处理办法是重新拿锁后**先 `FindChunkIdx` 确认它还在**
（返回 `chunkCount` = 已被收走，直接跳过），确认在才摘。

> 另：`SlabBackend::own(ptr)` 只对自己的区间表做二分，**从不解引用 ptr**，
> 所以门面可以放心地对任意外部指针（含 CRT 指针、栈地址）调它。

### 三、连带改动

| 文件                                                   | 动作                                                               |
| ------------------------------------------------------ | ------------------------------------------------------------------ |
| `Memory/XMemBackend.h`                                 | `+ SlabBackend`（约 +50 行注释 + 声明）                            |
| `src/Memory/XMemBackend.cpp`                           | **新建**，SlabBackend 实现                                         |
| `Memory/XMemFacade.h`                                  | `AllocHeader` 改 32B；`+ Checksum()`；`- m_OwnedTable`             |
| `src/Memory/XMemFacade.cpp`                            | 去表、加验真、`backendFor` 懒建 Slab 单例                          |
| `Memory/XMemOwnedSet.h`、`src/Memory/XMemOwnedSet.cpp` | **删除**（−214 行）                                                |
| `Memory/XMemTypes.h`                                   | Slab 注释"待迁移" → "已接入"                                       |
| `src/Memory/Buffer.cpp`                                | `include "../../Memory/XMemory.h"` → `"../../Memory/XMemFacade.h"` |

- `owns()` 改成"后端 own() + header 三连验真"，并**新增 `ownsFast()`**
  （只问后端、不读 header，绝不触碰外部内存）；`ownedCount()` 原来读表的元素个数，
  **改读统计器**（语义不变：在册 = 分配了还没释放，且跨基线也不会跑偏）。
- **顺带修掉旧 `Memory::Free` 的一个真 bug**：>64KB 独立块释放时
  `uint64_t size = 0; // 无法知道 malloc 的大小，只减计数` ——
  代码注释自己都写了"理想情况应该记录大小"，实际效果是 `UsedBytes` **减不掉**。
  新体系 header 里有 size，天然没这毛病。

### ⚠️ 一个要砚台处理的构建问题（我没动）

`build/CMakeFiles/XCore.dir/.../Memory/` 里**同时有旧的 `XMemory.cpp.obj`** ——
说明上次 `cmake` configure 时旧的 `XMemory.cpp` 还在（它是 ② 之前就存在的文件，
一直被 `file(GLOB)` 收着）。后果：

- `XMemory.cpp` 与 `XMemFacade.h` 定义了**两个 `X_Y::Memory`**（一个是单例 `Instance()`，
  一个只有 `Allocate/Deallocate`），且 `XMemory.cpp` 与 `Buffer.cpp` 都定义 `Memory::Alloc/Free`
  → 靠链接器"取第一个定义"决定行为，**这是个定时炸弹**；
- `OwnedSet.cpp` 删除后，旧 `.obj` 仍会留在库里。

**建议**：删掉 `Modules/XCore/src/Memory/XMemory.cpp` 与 `Modules/XCore/Memory/XMemory.h`
（旧体系已被新体系完全覆盖，且 `XMemBackend.cpp` 已把它的 slab 能力接过来），
然后重新 configure 一次让 GLOB 刷新。**这一步等砚台点头再动。**

### 四、验证（交给砚台）

- `MallocFrom(BackendType::Slab, n)` 若干轮 → `stats()` 里 Slab 槽有 used/alloc/cap；
- 释放后 `liveBlocks()` 回到 0，且 `ownedCount()` 与它一致；
- 重复 `Free` 同一指针 → 第二次被 magic 挡下并告警（不会二次归还）；
- `>64KB` 走 Slab 不崩（兜底 `::malloc`）；
- `Memory::Instance().setEnabled(false)` 退路仍有效。


## 2026-09-14 窗口存储位置显式表态：StorageTag（默认 Heap，栈对象写 StorageTag::Stack）

> 砚台："我建议你添加一个属性，让用户自己决定是栈上的还是堆上的，
> 然后我们默认是堆上，这样子用户自然而然就能意识到这个问题。"
> 背景：上一轮的自毁机制会对 `WindowDestroy` 做 `delete this`，
> 而 `test/main.cpp` 里原本是栈对象 → 一点关闭按钮就 delete 栈内存 = 崩。

### 设计

```cpp
// XWidget.h
enum class StorageTag
{
    Heap,  // new 出来（默认）—— 关窗时自动 delete，不泄漏
    Stack  // 栈/成员对象 —— 关窗时只退订，内存归作用域
};

explicit XWidget(XWidget* parent = nullptr, StorageTag storage = StorageTag::Heap);
bool IsHeapAllocated() const;
```

用法对比：
```cpp
auto* w = new TabContainer();                      // 默认 Heap → 关窗自动回收
TabContainer w(nullptr, X_Y::StorageTag::Stack);   // 显式栈 → 关窗只退订
```

**⚠️ 默认 Heap 是有意的**：让"这个窗口我要不要自己管"在写代码时就被看见 ——
栈对象必须显式敲 `StorageTag::Stack`，等于强制确认一次。

> 📌 初稿额外加了个 `struct StackTag{}` 作为"好写的栈标记"，砚台指出**多余** ——
> 直接用 `StorageTag::Stack` 就行，两套写法语义重复、还要多记一个类型。
> 已删除，现在只有一种写法。

### 自毁分支（关键）

```cpp
void XWidget::OnNativeDestroyed()
{
    if (m_RecycleQueued) return;
    m_RecycleQueued = true;

    // 栈对象：只退订，绝不 delete（内存归作用域）
    if (!IsHeapAllocated()) {
        disConnect(this);
        return;
    }
    // 堆对象：死前退订 → 入队 → 安全点 delete
    disConnect(this);
    app->DeferRecycle([this]{ delete this; });
}
```

### 连带改动

- `Container` 同步加两个构造（`StorageTag` 默认 Heap + `StackTag` 重载），
  `TabContainer` / `TabHostContainer` 用 `using Container::Container` 继承，
  **自动支持两种写法**，无需各改一遍。
- `test/main.cpp`：三个正式窗口保持 `new`（默认 Heap，不写 StorageTag 也走堆），
  另加一条 **自检 3/3**：栈上建窗口显式传 `StorageTag::Stack` → `show` → `destroy`，
  验证"没被误当堆对象 delete"（若识别失败，进程当场崩，测试即失败）。

### 实测（三路径行为）

```
① 默认（不表态）：  storage=Heap   IsHeap=1  → 堆对象 → 入队 delete   ✓
② 显式 Heap：      storage=Heap   IsHeap=1  → 堆对象 → 入队 delete   ✓
③ 显式 StackTag：  storage=Stack  IsHeap=0  → 只退订，不 delete；
                                              正常离开作用域才析构     ✓
exit=0
```

### 验证

- 最小复现：三条构造路径的 `IsHeapAllocated()` 与自毁分支行为均正确。
- `XWidget.cpp` / `Container.cpp` / `tabcontainer.cpp` / `tabhostcontainer.cpp` /
  `test/main.cpp` `g++ -std=c++20 -fsyntax-only -DXY_DEBUG` 全部通过。

> 📌 遗留：`storage` 参数目前**没有运行期校验**（无法检测"声明 Heap 但实际建在栈上"
> 这类说谎）。默认值和显式标记已能覆盖绝大多数误用，真正的强制手段
> （如私有化 `operator new`）留待以后需要时再谈。

---

## 2026-09-14 disConnect 单参版语义改定：删自己相关的【所有】订阅

> 砚台："我希望你改一下 disconnect 的行为，一个参数的那个就是删除自己作为接受者和发送者的订阅都删除。"

### 改前 / 改后

```cpp
// 改前：只比 receiver
void disConnect(MovementReceiver receiver) {
    if (it->receiver == receiver) erase;
}

// 改后：sender 或 receiver 任一命中即删
void disConnect(MovementReceiver receiver) {
    if (it->receiver == receiver || it->sender == receiver) erase;
}
```

**语义**：传进来的 ptr 只要出现在某条绑定的 **sender 或 receiver** 任一位置，这条绑定就删。
一句话 —— **"把我和事件系统的所有关系一次清干净"**。

### 为什么这么改

改前只比 receiver，能删掉 `connect(this, t, this, ...)`（this 兼作两者），
但会**漏掉 `sender == this && receiver != this`** 那类（本对象发事件给别人处理）。
对象要销毁时漏网 = dispatcher 里残留指向野指针的绑定 = **UAF**。

改后调用方只需 `disConnect(this)` **一次**，不必再记得补 `disConnect(self, self)`。

### 实测（4 条绑定的组合场景）

```
注册 4 条：
  [s=self  r=self  t=1]   自收自发（最常见）
  [s=self  r=other t=2]   自己发、别人收   ← 改前会漏网
  [s=other r=self  t=3]   别人发、自己收
  [s=other r=other t=4]   完全无关

disConnect(self) 后：
  [s=other r=other t=4]   ← 只剩这条无关的
>>> ✓ 与自身相关的 3 条全删干净
```

### 连带简化

- `XWidget::OnNativeDestroyed()`：原来的 `disConnect(self, self); disConnect(self);` 两句
  → **`disConnect(this);` 一句**。
- `XWidget::~XWidget()`：兜底退订，同样一句 `disConnect(this)`。
- `Container::~Container()`：原有 `disConnect(this)` **不用改**，但新语义下会顺带
  清掉 sender 身份那批 —— 正是壳死了想要的效果（改进，非破坏）。
- `XWidget::disconnectPa()` 用的是双参版，不受影响。

### 验证

- 最小复现：4 条绑定组合场景，`disConnect(self)` 精确删掉 3 条、保留无关的 1 条 ✓
- 4 个文件（XWidget / Container / Application / test main）`-fsyntax-only` 通过。

---

## 2026-09-14 ⚠️ 关键修复：destroy() 把自毁绑定删掉了 → 窗口照样泄漏

> 接上条（窗口自毁机制）。写完测试去跑，**发现自毁机制在 `destroy()` 路径上根本没生效**。
> **是砚台让写测试才炸出来的。**

### 问题

```cpp
// 老代码
void XWidget::destroy() {
    disConnect(this);      // ← 删掉【所有 receiver == this】的绑定
    this->Destroy();       // ← 然后才 DestroyWindow → 发 WindowDestroy 事件
}
```

`disConnect(this)` 会**把构造函数里新注册的这条也删掉**：
```cpp
connect(this, MovementType::WindowDestroy, this, &XWidget::OnNativeDestroyed);
//                                         ^^^^ receiver 就是 this
```
⇒ `DestroyWindow` 随后发出的 `WindowDestroy` 事件**找不到任何监听者** ⇒
`OnNativeDestroyed` 永远不被调用 ⇒ **对象永远不入回收队列 ⇒ 照样泄漏**。

**实测复现**（最小复现，模拟 destroy 顺序）：
```
注册 WindowClose + WindowDestroy 两条绑定：绑定数=2
disConnect(self) 之后：绑定数=0        ← WindowDestroy 也被删了
派发 WindowDestroy: handled=0, destroyed 计数=0
>>> 确认：OnNativeDestroyed 收不到事件，对象会泄漏
```

**影响面**：所有走 `destroy()` 的路径 —— 包括 `tabhostcontainer.cpp` 里刚改的两处
（`window->destroy()`）！也就是说：**上一轮"修好"的 L1/L2 其实没修好。**

### 修法

`destroy()` **不再退订**，退订完全交给 `OnNativeDestroyed()`（对象真要死时才断链），
外加 `~XWidget()` 兜底（覆盖"从没收到 WindowDestroy 就死了"的路径，如没 show() 过就被 delete）：

```cpp
void XWidget::destroy() {
    // 不能 disConnect(this)！会把 WindowDestroy 绑定一起删掉。
    this->Destroy();
}
XWidget::~XWidget() { disConnect(this); }   // 兜底
```

老代码那个 `disConnect` 附带作用是"防重复关闭"，但 `Destroy()` 内部本就有
`if (m_Hwnd)` 保护，重复调用是 no-op，不影响。

**修复后实测**：
```
修复后 destroy() 不退订 -> 绑定数=2
派发 WindowDestroy: handled=1, destroyed=1
>>> ✓ 自毁回调收到了，对象会被回收
```

### ⚠️ 教训（给我自己）

**机制写完了不等于接上了。** 上一轮只验证了"`OnNativeDestroyed` 里的逻辑对"，
没验证"`destroy()` 之后它到底会不会被调用"。
以后这类"跨函数协作"的改动，必须端到端跑一遍，不能只看局部。

### test 工程：改成自检程序

`test/src/main.cpp` 启动时自动跑两项检查（宏 `XY_SELFCHECK_WINDOW_RECYCLE`，默认 1）：

- **自检 1/2 窗口自毁 + 延迟回收**：`ProbeWindow`（记构造/析构计数）——
  `new → show → destroy()`，断言 `destroy()` 返回后**尚未析构**（延迟生效）、
  跑几轮 `ProcessEvents` 后**析构计数 +1**（安全点真的 delete 了）。
- **自检 2/2 派发中退订**：同一 sender 挂 3 个 handler，H1 里 `disConnect` 掉自己，
  断言 H1=H2=H3=1（改前会崩在 `bad_function_call`，或 H2 被跳过）。

三个正式窗口（top/log/hex）已从**栈对象改成 `new`** —— 自毁机制会对 `WindowDestroy`
做 `delete this`，栈对象必崩。（`topLayout` 也 new，因为 `SetDockLayout` 会
`m_Layout.reset(layout)` 接管所有权。）

埋点日志（`XY_DEBUG`）：`OnNativeDestroyed` 打印「收到 WindowDestroy → 登记延迟回收 /
已入队 / 延迟回收执行 → delete this」；`FlushDeferredRecycle` 打印
「回收安全点：本轮待回收动作 N 个」。

> ⚠️ test 链接的是 `dist/lib/mingw` 预编译库 + `dist/include` 头文件（**拷贝快照，非软链**），
> **必须重新构建 libMovement / libWidget / libUI 并同步 dist**，改动才生效。

### 验证

- 最小复现：改前 `destroyed=0`（漏），改后 `destroyed=1`（回收）。
- `XWidget.cpp`、`test/src/main.cpp` `g++ -std=c++20 -fsyntax-only -DXY_DEBUG` 通过。

---

## 2026-09-14 MovementDispatcher 派发中退订崩溃：DispatchEvent 改快照（砚台问出来的）

> 砚台问："所以我的 disconnect 到底有没有问题？"
> 顺着查了一遍 —— **三个 disConnect 重载本身逻辑都对**（比较字段无误），
> 但**派发侧有个必现的崩溃 bug**，而且是"平时不炸、特定顺序才炸"那种。

### 问题：DispatchEvent 边遍历边让回调改 m_Bindings → 迭代器失效

```cpp
// 旧（movements.h，改前）
for (auto &bind : m_Bindings)          // 范围 for = 迭代器遍历
{
    if (匹配) {
        bind.handler(*event);          // ← 回调里可能调 disConnect → m_Bindings.erase
        handled = true;
    }
}
```

**这不是理论风险，是实际会走的路径**：
`XWidget::destroy()` 里有 `disConnect(this)`，而 `destroy` 正是
`WindowClose` 的 handler（`XWidget.cpp:21/31`）—— 也就是说**"窗口关闭"这个最普通的操作
就会在派发遍历中删绑定**。同一 sender 挂了多个 handler 时必崩。

**实测复现**（写了个最小复现，同一 sender 注册 3 个 handler，H1 里执行 disConnect）：
```
H1 执行, 准备 disConnect(&a)
H3 执行                      ← H2 被跳过了（元素前移错位）
terminate called after throwing an instance of 'std::bad_function_call'
exit=3                       ← 崩溃
```

### 修法：派发前快照（砚台拍板方案 A）

```cpp
// 新：先收集命中的 handler 拷贝，再遍历快照
std::vector<MovementHandler> pending;
for (const auto &bind : m_Bindings)
    if (匹配) pending.push_back(bind.handler);   // 拷贝，与 m_Bindings 脱钩

for (auto &handler : pending)
    if (handler) { handler(*event); handled = true; }
```
回调里对 `m_Bindings` 的任何增删都只作用于真实表，不影响本轮遍历。

**实测修复后**：H1/H2/H3 全部正常执行，`exit=0`，无崩溃，剩余绑定数正确。

### 语义（有意如此，写进注释）

- 快照 = **派发开始那一刻在场的监听者**。
- 回调期间**新注册**的 handler **不收到本次事件**（事件发给当时在场的人）。
- 回调期间**被摘掉**的 handler，若已在快照里则**仍会被调用一次**（拷贝出来时它还是有效订阅）。

### 顺带记录（本次未修，已列入 MemoryAudit）

- `g_TypeIdNext` 是**非原子**的自增（`movements.h:221`），多线程首次调用理论上可撞 id。
- `m_Bindings` 是**裸 `std::vector` 无锁**：`DispatchEvent` 在主线程，
  而 `Connect`/`disConnect` 可能被 Ticker 线程调。
- **本次按砚台指示只修 #1**，上面两条留待办。

### 验证

- 最小复现：改前崩（exit=3 / bad_function_call），改后正常（exit=0）。
- 9 个相关 .cpp 文件 `g++ -std=c++20 -fsyntax-only` 全部通过（语法自检，**非项目构建**）。

---

## 2026-09-14 Widget 层补"窗口自毁"：Application 通用延迟回收队列

> 起因：砚台发现 `tabhostcontainer.cpp` 里 `new TabContainer()` 建的独立窗口**没人 delete**。
> 追下去发现根因不是"忘了写 delete"，而是 **Widget 层压根没提供"窗口对象该何时死"的机制** ——
> `Destroy()` 只销毁 HWND（`WindowImplWin32.cpp:122` 就一句 `DestroyWindow`），
> `Close()` 更是只 `Show(Hide)`，全仓库唯一的 `delete this` 是 COM `FileDropTarget::Release()`，跟窗口无关。
> 上层只能二选一：自己管生命周期（还要处理消息循环时序），或者漏。

### 砚台拍的板

1. **窗口一律 `new`** —— 不加"自毁开关"，约定写进注释。
2. **通用回收**：队列存 `std::function<void()>` 而非 `void*`
   （`void*` 丢类型，多态对象用基类指针 delete 是 UB + 泄漏；
   通用性落在"怎么回收"这半 —— 窗口 `delete this`、句柄 `CloseHandle`、GL 对象都能入队）。
3. **执行时机**：每轮 `ProcessEvents` 末尾。
4. 关闭按钮语义 **保持现状**（关闭即销毁），三条 `WindowClose` connect 一行未改。

### 改动

**① `Application.h/.cpp` —— 通用延迟回收队列**

```cpp
std::vector<std::function<void()>> m_DeferredRecycle;
void DeferRecycle(std::function<void()> action);   // 入队（只对 new 出来的对象用）
void FlushDeferredRecycle();                       // 执行并清空
```

- `FlushDeferredRecycle` **先 swap 取走再执行**：动作本身可能又入队（如析构期间又关了别的窗口），
  边遍历边 `push_back` 会迭代器失效。
- 调用点：`ProcessEvents()` 的 `while` **之外**（本轮事件全部派发完、所有回调栈帧已返回）。
  在循环内 flush 会让队列里其它事件引用到已回收的 sender。
- `~Application()` 兜底 flush 一次（窗口已 Destroy 但事件循环没再转一轮的情况）。

**② `XWidget.h/.cpp` —— 自毁语义放在 XWidget，不写进 Win32**

构造函数新增（平台无关）：
```cpp
connect(this, MovementType::WindowDestroy, this, &XWidget::OnNativeDestroyed);
```
- 原来只监听了 `WindowClose`，**`WindowDestroy` 事件一直在发但没人接**。
- ⚠️ 为什么不写在 `WM_NCDESTROY` 里：将来接 X11 / Cocoa，只要它的消息泵也发
  `WindowDestroy`，自毁自动生效，**不用改任何平台代码**。
- `OnNativeDestroyed()` 只登记延迟回收，**不当场 `delete this`** ——
  它跑在 dispatcher 的绑定遍历中，`ProcessEvents` 也还在栈上且持有 `this`。

**③ 防 double delete 门闩**

```cpp
bool m_RecycleQueued = false;   // XWidget 私有
```
同一对象入队两次 → flush 时 `delete` 两遍 → 崩溃。
`WM_DESTROY` 正常只发一次，但将来若有别的路径也调 `OnNativeDestroyed` 就危险，故加闩。

**④ 死前退订（两个重载比的东西不同）**

`movements.h` 里两个 disConnect 重载：
- 双参 `disConnect(sender, receiver)` —— 比 **sender && receiver**
- 单参 `disConnect(receiver)` —— **只比 receiver**

**实测**（写了个最小复现跑的，不是推测）：
- 构造函数里注册的 `connect(this, type, this, ...)` 是 `sender == receiver == this`，
  **单参版本来就能删掉它**（receiver 字段就是 this）。
- 但单参版**会漏掉 `sender == this && receiver != this`** 那类绑定
  （如"本窗口发事件给别的对象处理"）。
- 双参版专治这条漏网的。

⇒ 两条各删各的，**合起来才彻底**：
```cpp
void *self = this;
disConnect(self, self);   // sender == this && receiver == this
disConnect(self);         // 所有 receiver == this（含 sender 是别人的）
```
（`MovementSender` / `MovementReceiver` 都是 `void*` 同一类型，实测 `disConnect(p, p)` 不产生重载歧义。）

> 📌 初稿曾把理由写成"单参版删不掉 sender 身份那批" —— **那是错的**，
> 复核 `movements.h:164` 的实现并实测后已更正为上面这版。

**⑤ `tabhostcontainer.cpp` —— 真正修掉 L1/L2**

去掉两处 `delete window`（老代码只销毁 HWND，C++ 对象含 `m_Layout`→Dock→Panel 整棵树根本没析构，
**反而更漏**），改走 `window->destroy()` → `DestroyWindow` → `WM_DESTROY` → `WindowDestroy`
→ XWidget 自动登记回收。

`tabcontainer.cpp:67` 的 `destroy()`（目标窗口收养成功、源窗口自我退场）**本来就是对的**，
不改 —— 它正是这套机制的目标场景。

### 行为变化

- **拖出面板建的独立窗口**：不再泄漏（原来每次拖出一份 `TabContainer` + 整个布局树）。
- **"拖出失败"时窗口不是立刻消失**：`HandlePanelDrop` 跑在事件回调里，
  `destroy()` 只是登记，真正回收在下一轮 `ProcessEvents` 末尾 —— **这正是延迟回收的意义**。
- 关闭按钮行为**未变**（关闭即销毁 → 触发自毁回收）。

### 验证

- 5 个文件 `g++ -std=c++20 -fsyntax-only` 全部通过（语法自检，**非项目构建**）。
- 运行时待砚台验：拖出面板 → 关掉独立窗口 → 看有没有崩溃/泄漏
  （建议在 `~TabContainer` 里打一行日志确认析构真的跑到了）。

### 遗留

- `XWidget::destroy()` 的命名仍容易误导（名字像"销毁对象"，实际只到 `DestroyWindow`）。
  这次没动 —— 等砚台决定要不要改名。
- 审计里其余项（U2/U5 断链、M2、T4 Join 无超时）未动。

---
