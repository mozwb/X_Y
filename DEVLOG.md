# DEVLOG

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

## 2026-09-14 UI 资源管理审计 + 修两处内存/关闭 bug

> 砚台："检查一下 UI 模块的资源管理，看看有没有泄露的风险。"
> 审计结论与完整清单见 `_notes/arch/MemoryAudit.md` **附录 A**（本次新增）。
> 本条目只记**本次实际动过的代码**。

### 审计结论（先说好消息）

四层所有权已按"方案 2"落地（`DockLayout::m_OwnedDocks` / `Dock::m_Panels` /
`Panel::m_Components` / `Container::m_Layout` 全是 `unique_ptr`），
`HexViewer::m_FileTabs`、`FontLibrary`、`CanvasImplWin32` 的 DIB/DC、
`EnableFileDrop` 的 OLE 引用计数**都干净**。UI 层不直接持有任何 GDI/内核句柄。

剩下的问题分三类，**本次只修了两处**：

| 类别 | 条目 | 本次 |
|------|------|------|
| 真泄漏 | L1/L2 拖出面板建的独立窗口对象无人 delete | ✅ 同日第二条已修 |
| 数据增长 | **M1 LogStripe 只增不删、无上限** | ✅ **已修** |
| 关闭正确性 | **HexViewer 延迟关闭存下标会过期（关错文件）/ 单槽会覆盖（丢关闭）** | ✅ **已修** |
| 悬空借用 | U1~U7（`m_FileDropPanel` / `ScrollArea::m_Content` / `m_DragPanel` …） | ⏸ 未修 |
| 自毁路径 | Split/Merge/RemovePanel | ⏸ 砚台明确暂不处理 |

### 改动 1：LogStripe 封顶 5000（方案 A）

**问题**：上游 `m_AllEntries` 是 `LoopQueue<LogEntry,5000>`（环形，满了覆盖最旧，内存恒定），
但下游 `m_LogStripe`（`ListBox`，`std::vector<ListItem>`）**只加不删** ——
上游封顶、下游不封顶。开几小时 stripe 涨到几十万条，内存全在这儿；
且 `RebuildAll()` 全量重建时峰值翻倍 + 卡顿。
`MAX_ENTRIES` 过去只管住了 `m_AllEntries`，**管不到 stripe** —— 这是最容易误读的点。

**改法**（砚台拍板方案 A：给 ListBox 加通用原子能力，而不是整表 Clear 重灌）：

- `Modules/UI/Component/ListBox.h` + `src/ListBox.cpp`：新增 **`RemoveFirst(int n)`**
  —— 删头部最旧 n 条。
  ⚠️ 关键是它**必须重置折行游标**：`m_FoldedCount` 是"已惰性折叠到第几条"的游标，
  `EnsureFold` 只从它往后补算 —— 这个设计**只适用于只增不减**。头部一删，
  所有 item 下标前移，`m_Fold[i]` 与 `m_Items[i]` 整体错位。
  ⇒ 统一重置（`m_FoldedCount=0; m_LastFoldWidth=-1;`）触发一次全量重折。
  同时维护 `m_SelectedIndex`（平移 n；被删掉则收敛到 0 或 -1）。
- `Modules/UI/Panel/LogViewer.h` + `src/LogViewer.cpp`：新增私有 `TrimStripe()`，
  在 `IncrementalAppend` 与 `RebuildAll` 末尾调用，把 stripe 钳到 `MAX_ENTRIES(5000)`。

### 改动 2：HexViewer 延迟关闭改用指针 + 队列

**问题**：关闭是**延迟**的（`Button::OnInput` 正在 `Horizontal::Components` 的遍历栈里，
此刻删自己会 UAF）—— 这个延迟设计本身是对的。但登记的东西不对：

```cpp
// 旧：捕获【下标】，且只有【单槽】
std::size_t m_PendingClose = -1;
tab->SetOnClose([this, index]{ m_PendingClose = index; });
```
1. **下标会过期**：登记到结算之间若发生任何改动 `m_Files` 的操作
   （拖入新文件、关掉别的 tab），下标指向的已不是当初点 × 的那个文件 → **关错文件**。
2. **单槽会覆盖**：两次点 × 之间没发生重绘，后一次覆盖前一次 → **丢关闭请求**（点了没反应）。

**改法**：
- `Modules/UI/Panel/HexViewer.h`：`std::size_t m_PendingClose` →
  `std::vector<Button *> m_PendingClose`（指针稳定 + 队列不丢）。
- `Modules/UI/src/HexViewer.cpp`：
  - `AddFile` / `CloseFile` 的 `SetOnClose` 统一改为**按指针捕获**（两处写法归一，免得再走岔）。
  - `ProcessPendingClose()`：`swap` 取走队列（避免自己吃自己），逐条**按指针反查当前下标**再 `CloseFile`。
  - `ClearFiles()`：**先 `m_PendingClose.clear()`** —— 否则紧随其后的 `m_FileTabs.clear()`
    会把队列里的裸指针全变悬空，下次结算即 UAF。

### 验证

- 三个文件 `g++ -std=c++20 -fsyntax-only` 全部通过（语法自检，**非项目构建**）。
- 运行时行为（折行重算、选中平移、连点多个 × 是否都关掉）**待砚台跑起来看**。

### 遗留（下次可做，都还没动）

- U2 `DockLayout::m_FileDropPanel` / U5 `ScrollArea::m_Content` 断链（各单文件，机械补）。
- M2 `HexViewer` 每个打开文件整份常驻；M3 `SetDataStoreKey` 全量重建峰值。
- T4 `LogViewer::Stop()` 的 `Join()` 无超时（UI 线程可能永久卡死）。

---

## 2026-09-13 Panel 转移改成纯 move（unique_ptr 全程在手）

> 砚台问："`m_Panels` 是拥有还是借用？那 panel 咋转移呢？"
> 借此把转移路径上的"无主裸指针窗口"也消灭掉。

### 改了什么

之前 `DetachPanel` 摘出的所有权**经由裸指针**交给下一个 Dock：

```cpp
Panel *DetachPanel(...);        // 所有权离手，但没有任何东西记录谁接着
// ... 中间若早退 / 抛异常 / 忘了接管 → 泄漏，编译器不提醒
```

现在**全程在 `unique_ptr` 手里**：

```cpp
std::unique_ptr<Panel> Dock::DetachPanel(Panel *panel, std::string *title);
Panel *Dock::AddPanel(std::unique_ptr<Panel> panel, const std::string &title);
```

| 层 | 新签名 |
|----|--------|
| `Dock` | `AddPanel(unique_ptr)` 接管 / `DetachPanel -> unique_ptr` 交出 / `TakePanelInternal -> unique_ptr` |
| `DockLayout` | `AddPanelAt(unique_ptr, x, y, title)` |
| `Container` | `DetachPanel -> unique_ptr` |
| `TabDock` | `DetachToWindowHandler` 回调签名改为收 `unique_ptr<Panel>` |
| `TabContainer`/`TabHostContainer` | `HandlePanelDrop(unique_ptr<Panel>, ...)` |

### 顺带简化

- `Dock::RemovePanelInternal(Panel*)`（void，靠 release 裸指针）→
  **`TakePanelInternal(Panel*) -> unique_ptr`**。`Split` 里变成纯 move 链：
  ```cpp
  if (auto owned = TakePanelInternal(active))
      newDock->AddPanel(std::move(owned), "");
  ```
- `TabDock::DropPanel` 里不再需要"失败就 AddPanel 放回去"的补救分支 ——
  回调改成收 `unique_ptr` 后，接管责任在回调内部，语义更清楚。

### 验证（`g++ -std=c++20`，20 个 UI 源文件全部编译通过）

运行时逐条验证，重点是**老版本保证不了的两条**：

| 场景 | 结果 |
|------|------|
| `AddPanel(unique_ptr)` 接管 → 析构释放 | ✅ `alive 1→0` |
| `DetachPanel` 跨 Dock 转移 → 恰好死一次 | ✅ `alive 1→0`（无双重释放/泄漏） |
| **★ DetachPanel 后 `owned.reset()`（模拟忘了接管/中途早退）** | ✅ **自动释放，无泄漏**（老版本此处必漏） |
| **★ `AddPanel` 被拒（`CanAddPanel` 为假）** | ✅ **unique_ptr 自动释放**（老版本此处必漏） |
| `Split` 的 move 链（活跃面板随新 Dock） | ✅ `a.size=0 b=1 alive=1`，析构后 `alive=0` |

### ⚠️ 语义变化（回调约定）

`DetachToWindowHandler` 现在**按值收 `unique_ptr`**：

- 返回 `true` = 回调已接管（所有权归它）
- 返回 `false` = **此时 panel 已随参数析构**（`AddPanelAt` 失败时它释放了）
  ⇒ 所以回调返回 false 时不能再去用那个 panel；`TabDock::DropPanel` 也因此
  去掉了"失败就放回自己"的分支。

### 仍需同步的 test 代码

`test/src/main.cpp`（不在本仓库）：`delete logViewer/hexViewer` 仍是双重释放；
`TopLayout topLayout;` 传 `&topLayout` 给 `SetDockLayout` 会被 delete（栈对象）。

## 2026-09-13 UI 所有权显式化（④ 修正：模型定了，我上一条写错了）

> ⚠️ **本条修正同日上一条 ④ 的所有权模型描述。** 我上一轮照着自己臆想的 UML 图
> 改了代码，引入了根本不存在的"借用 layout / 借用 dock"概念。砚台指出后重做。

### 我错在哪（记下来，免得再犯）

| 我臆想的 | 实际 |
|---|---|
| `TopLayout` 是"外部传进来的栈对象" → Container 只能**借用**它 | `TopLayout` 是 `DockLayout` 的**便利子类，它就是这个窗口的 layout**（一窗口一 layout） |
| Dock 分"内建（借用）/动态（拥有）"，要两套列表 | Dock **全部归 layout**，不分来源 |
| `Container` 需要 `m_LayoutOwned` + 借用 `m_Layout` 两个成员 | 只需要一个 `unique_ptr<DockLayout>` |

**根因**：把"谁拥有"和"谁在管布局"混成一件事，于是造出多余的抽象。

### 正确模型（砚台确认）

```
窗口 (Container)
  └── 一个 DockLayout                ← 一窗口一 layout，唯一
        ├── 无宗 Dock（初始五区域）    ┐ 全部归 layout，无借用概念
        └── 有父 Dock（Split 切出）    ┘ （区别只在"相对位置固定 / 归还方式"）
              └── Panel               ← 唯一会"搬家"的东西
```

- **Dock 不会搬家**（只在宗族内 Split/Merge），所以 Dock 层不存在借用。
- **只有 Panel 会转移**（在 Dock 之间拖拽）→ **借用只存在于 Panel 这一层**。
- 无宗 Dock = 窗口最初始的区域划分，相对位置不可变更。
- 有父 Dock 由 `DockFather` 记录宗族；归还由宗族处理（`Merge`，已实现，本次未动逻辑）。

### 本次改动

| 文件 | 从（我上一轮写错的） | 到（正确模型） |
|------|---------------------|---------------|
| `DockLayout` | `m_Docks`（借用）+ `m_OwnedDocks`（拥有） | `m_OwnedDocks`（全拥有）+ `m_Docks`（借用视图） |
| `DockLayout` | `AddDock`（借用）/ `AddOwnedDock`（拥有）两个入口 | **统一 `AddDock`（接管所有权）**；删 `AddOwnedDock` |
| `DockLayout` | `RemoveDock`（只摘）+ `RemoveAndDestroyDock`（摘+删） | **统一 `RemoveDock`（摘 + 释放）**；删 `RemoveAndDestroyDock` |
| `Container` | `unique_ptr m_LayoutOwned` + 借用 `m_Layout` | **单个 `unique_ptr<DockLayout> m_Layout`** |
| `TopLayout` | 五个 `TabDock` **值成员** | 五个 `new TabDock()`，**所有权归基类**；成员改指针（借用视图），`TopDock()` 返回引用 |
| `Container::AddSinglePanel` | `AddDock` 后再 `DockBind`（重复登记） | 只调 `DockBind`（内部走 AddDock，一次到位） |

### 保留的上轮修复（这些是真 BUG，与模型无关）

- **`Dock::Merge()` 自毁后继续访问 `this`**：老代码 `RemoveDock(this)` 后又调
  `RequestRepaint()`（访问成员），而注释说"随后由布局释放"却没人释放
  —— use-after-free + 泄漏。现在先抄下 `layout` 指针，最后一行 `RemoveDock(this)`，
  并注明此后不得再碰任何成员。
- **借用指针断链**：`Dock::TakeEntry` 清 `m_MouseCapturePanel`；
  `TabDock::DropPanel` 在 `DetachPanel` 后立刻 `ResetPanelDrag`；
  `Panel::RemoveComponent` 先断焦点/拖拽目标再释放；
  `DockLayout::RemoveDock` 清 `m_MouseCaptureDock` / `m_FileDropPanel`。
- **`Dock::TakeEntry`**：三处重复的下标维护收敛成一份。

### 验证

- **20 个 UI 源文件全部编译通过**（含 imgui 两个）
- 运行时（独立测试程序）：
  | 场景 | 结果 |
  |------|------|
  | layout 析构释放全部 Dock | ✅ `dockDead=2` |
  | **`TopLayout` 五个区域 Dock 由 layout 释放** | ✅ 全局 new/delete 计数 **`net=0`**（无泄漏） |
  | `Dock` / `TabDock` 有虚析构，经基类指针 delete 正确 | ✅ `has_virtual_destructor = 1` |
  | Dock 析构释放全部 Panel | ✅ `panelDead=2` |
  | Panel 跨 Dock 转移 | ✅ `panelDead=1`（恰好一次，无双重释放） |
  | Panel 拥有 Component | ✅ 无泄漏 |

### ⚠️ 破坏性变更（需同步）

- `test/src/main.cpp`：`TopLayout topLayout;` 是**栈对象** + `SetDockLayout(&topLayout)`，
  现在 `SetDockLayout` **接管所有权**、`Container` 析构会 `delete` 它
  → **栈对象被 delete，会崩**。该文件在 `D:\workbench\test`（不在本仓库），未改。
  正确写法：`topWindow.SetDockLayout(new X_Y::TopLayout());`
- 同文件结尾的 `delete logViewer; delete hexViewer;` 也是**双重释放**（所有权已归 Dock）。

## 2026-09-13 UI 所有权显式化（④：Container ⊃ Layout ⊃ Dock ⊃ Panel ⊃ Component）

> 承接内存审计（`_notes/arch/MemoryAudit.md`）。审计结论：UI 的隐患根因不是"漏了 delete"，
> 而是**所有权从未在代码里落地** —— 四层全是裸指针 + 不拥有，而真正的所有者
> 付不起 delete 的代价（重活都在析构里）。本步把所有权写进类型。

### 改动（按砚台定案的链路逐层落地）

| 层 | 改动 | 关键点 |
|----|------|--------|
| **Component** | 不改 | 叶子，无子节点 |
| **Panel** | `m_Components`：`vector<Component*>` → `vector<unique_ptr<Component>>` | **Panel 拥有组件**；`AddComponent` 即接管 |
| **Dock** | `m_Panels`/`m_Titles` → `vector<PanelEntry{unique_ptr<Panel>, string}>` | **Dock 拥有 Panel**；一个容器管住指针+标题，不再两处同步下标 |
| **DockLayout** | `m_Docks`（借用）+ 新增 `m_OwnedDocks`（拥有） | **区分内建/动态 dock**（砚台定案 (b)） |
| **Container** | `m_Layout` + `bool m_OwnLayout` → `unique_ptr<DockLayout> m_LayoutOwned` + 借用 `m_Layout` | 自建才释放；**外部传入（栈对象）绝不删** |

### 关键设计

1. **两套 Dock 列表（方案 b）**：`TopLayout` 的五个 Dock 是**值成员**，
   若混进拥有列表会被 delete 栈对象 → 立即崩。故：
   - `DockBind(dock&, ...)` → **借用**（引用传入 = 调用方持有）
   - `AddOwnedDock(dock*)` → **拥有**（指针传入 = 接管）
   - `Container::AddSinglePanel` / `Dock::Split` 一律走 `AddOwnedDock`。

2. **借用指针必须显式断链**（审计里的 [B]/[G]）：
   - `Dock`：`TakeEntry()` 统一维护下标 + **清 `m_MouseCapturePanel`**；
     拖拽转移时 `TabDock::DropPanel` 在 `DetachPanel` 后立刻 `ResetPanelDrag()`
     （否则 `m_DragPanel` 在两步之间悬空）
   - `Panel`：`RemoveComponent` 先断 `m_FocusedComponent`/`m_DragTarget` 再释放
   - `DockLayout::RemoveAndDestroyDock` 顺手清 `m_MouseCaptureDock`/`m_FileDropPanel`

3. **三处重复的下标维护收敛成一个 `Dock::TakeEntry(idx, title*)`**：
   `RemovePanel`/`DetachPanel`/`RemovePanelInternal` 原先各写一遍下标修正
   （三份都略有差异），现在共用一份，返回 `unique_ptr` 表达所有权转移。

### 顺带修掉的两个真 BUG

- **`Dock::Merge()` 自毁后继续访问 `this`**：老代码 `RemoveDock(this)` 之后又调
  `RequestRepaint()`（访问成员），而注释写着"随后由布局释放本 Dock"却**没人释放**
  —— 既是 use-after-free 又是泄漏。改为先抄下 `layout` 指针，最后一行
  `RemoveAndDestroyDock(this)`，并注明**此后不得再碰任何成员**。
- **`DockLayout::RemoveDock` 只摘不删**：动态 Dock（`Split` 产生的）从此无人释放。
  新增 `RemoveAndDestroyDock`（只释放自己拥有的；内建 Dock 会被跳过）。

### 验证（`g++ -std=c++20 -fsyntax-only`，按约定未做工程构建）

- **全部 20 个 UI 源文件编译通过**（含 imgui 两个）
- 运行时所有权行为（独立测试程序连 Panel/Dock/DockLayout）：
  | 场景 | 结果 |
  |------|------|
  | Panel 析构 → 释放全部组件 | ✅ `dead=3` |
  | Dock 析构 → 释放全部 Panel | ✅ 2 个面板恰好各死一次 |
  | **内建 dock（栈对象）未被 layout 删除** | ✅ `mixed ownership: survived, no crash` |
  | **`DetachPanel` 跨 Dock 转移 → 恰好死一次** | ✅ `dead=1`（无双重释放、无泄漏） |
  | `RemovePanel` 恰好释放一次 | ✅ `dead=1` |
  | `RemoveComponent` 断焦点借用后释放 | ✅ 不崩 |

### 尚未做（下一轮）

- **⑤**：`Shutdown()` 与析构分离（治 `LogViewer::~LogViewer` 里 `Join()` 卡死）
- **⑥**：回调链弱引用（`m_HostRepaint`/`m_RepaintCallback` 的断链时机）
- **⑦**：`Component` 内部反向借用（`TagStrip::m_Owner`、`ScrollArea::m_Content`）的清理约定
- ⚠️ **`test/src/main.cpp` 需同步**：`new LogViewer` 传给 `AddSinglePanel` 后
  所有权已归 Dock，结尾那两行 `delete logViewer/hexViewer` **现在是双重释放**
  （该文件在 `D:\workbench\test`，不在本仓库，未改）

## 2026-09-13 内存模块：实现与声明分离（可读性重构）

> 砚台反馈："`.h` 写了所有实现读起来有点困难了"。确实 —— 上一步 ③-a 把门面
> 和统计的实现全内联在头文件里，`XMemFacade.h` 一度 608 行、`XMemStats.h` 396 行，
> 接口被实现细节淹没。本步做分离，**行为零变化**。

### 改动

| 文件 | 前 | 后 | 说明 |
|------|----|----|------|
| `XMemFacade.h` | 608 行 | **329 行** | 只留接口 + 常量 + 命名空间级 inline 便捷函数 |
| `XMemFacade.cpp` | — | **183 行**（新） | `allocate` / `deallocate` / `ResolveWithSize` / `backendFor` / `Shutdown` |
| `XMemStats.h` | 396 行 | **177 行** | 只留 POD 快照 + `MemoryCounter` 声明 |
| `XMemStats.cpp` | — | **319 行**（新） | 计数、基线、快照、打印 |
| `XMemOwnedSet.h` | — | **53 行**（新） | 已分配指针表声明（从 Facade 抽出） |
| `XMemOwnedSet.cpp` | — | **144 行**（新） | 哈希表实现（纯实现细节，读门面时不必看） |

### 分离原则（写进各文件头注释）

- **留头文件**：类声明、常量、模板（语言要求）、以及**必须在静态初始化期就能调用**
  的极小路（`Instance()` / `enabled()` / 命名空间级 `Malloc`/`Free` 等 inline 转发）。
- **移到 .cpp**：分配头维护、预算检查、归属判定、后端分发、哈希表、统计与打印。

### 顺带修的问题

- `XMemFacade.h` 缺 `<exception>`（`std::terminate`）—— 之前靠别的头间接引入，
  分离后立刻暴露。已补。
- 顺手清掉 Facade 头里 `cstdio`/`cstdlib`/`cstring`/`exception` 等只在 .cpp 需要的包含。

### 验证

- `g++ -std=c++20` 全量编译（5 个 cpp：GlobalNew / Stats / OwnedSet / Facade + 测试）
- 行为对比前一步**完全一致**：
  - STL 容器全部进门面（作用域内 `live=103 owned=103`）
  - 2000 轮 `new`/`delete` 后 `live=0 owned=0 used=0`
  - 归属判定：门面指针 `owns=1`，栈指针 `owns=0`（不解引用外部指针）
  - 开关关闭后新分配走原生，释放仍安全
  - 统计四维度输出正常（按后端 / 按大小档 / 峰值 / 未回收块）

> 仍未接入 CMake（`XCore` 用 `file(GLOB)`，下次 configure 自动纳入）。
> 下一步待定：③-b（SlabBackend 迁入）或 ④（UI 四层所有权显式化）。

## 2026-09-13 内存模块 ③-a：全局 new 重载 + 三通道 + 策略 + 统计细化（仍未接入构建）

> 承接同日 ②（后端/统计/门面拆分）。本步加上"必经之路"，实现**统计所有分配**。

### 文件（②的三个文件已按砚台命名偏好改名为 `XMem*`）

| 文件 | 状态 | 职责 |
|------|------|------|
| `XCore/Memory/XMemTypes.h` | 🏗️ 新 | 公共枚举：`BackendType` / `OOMAction` / `SizeClass` + 分档函数 |
| `XCore/Memory/XMemBackend.h` | 🔧 改名 | `IMemoryBackend` + `CrtBackend`（原 MemoryBackend.h） |
| `XCore/Memory/XMemStats.h` | 🔧 改名+扩 | `MemoryCounter` / `MemoryStats` / `BackendStats` / `SizeClassStats` |
| `XCore/Memory/XMemFacade.h` | 🔧 改名+扩 | 门面 + `OwnedSet` + 策略 + 三通道（原 Memory.h） |
| `XCore/Memory/XMemGlobalNew.cpp` | 🏗️ 新 | 全局 `operator new/delete` 全套（含 `new[]`、nothrow、sized） |
| `XCore/Memory/XMemory.h/.cpp` | ⏸️ 未动 | 旧 slab，待迁进 `SlabBackend` |

### 定案：门面的三条通道（砚台拍板）

| 通道 | API | 用途 |
|------|-----|------|
| ① 统一兜底 | 全局 `operator new/delete` | **所有** new（含 STL/第三方）必经之路，没人能绕过 |
| ② 裸内存 | `X_Y::Malloc` / `X_Y::Free` | 替代 C 的 malloc/free（头文件已写醒目警告：**别混用**） |
| ③ 显式后端 | `X_Y::AllocFrom` / `X_Y::MallocFrom` / `X_Y::NewFrom<Backend, T>` | 单次指定后端，绕过策略 |

### 关键设计（本步新增的部分）

1. **全局 `operator new/delete` 重载**：默认后端 = CRT ⇒ 效果是"标准库堆 + 一层记账"，
   不是"换掉标准库"。行为对使用者透明，性能损耗 ≈ 一次调用 + 16B 分配头。
2. **`OwnedSet`（已分配指针表）**：全局 delete 会把**所有**指针送进门面，
   要安全回答"这指针是不是我发的"**不能读 ptr-16**（外部指针那里可能未映射 → 段错误），
   故自管一个开放寻址哈希表（线性探测 + 墓碑）。**只用 `::malloc`/`::free`**，守自举铁律。
3. **开关与释放解耦**：`setEnabled(false)` 只影响**新分配**；释放**永远**交门面判断归属。
   ⇒ 运行中切换开关**不会**造成"分配走门面、释放走 CRT"的错配。
4. **策略注入**：`setPolicy([](uint64_t n){ return n<=512 ? Slab : Crt; })`。
   策略**只影响分配**，释放始终读分配头 ⇒ 随时换策略、永不错配。
   ⚠️ 策略的 `std::function` 用 `malloc` 手工安置，避开它自身堆分配触发的自举递归。
5. **`markBaseline()`（统计去皮）**：解决启动期分配（全局对象 / CRT 自身）污染泄漏自检。
   含**跨基线释放下溢钳制** + `PreBaselineFrees` 计数。
6. **统计细化到四个维度**：按后端 / 按大小档（新增，为日后定策略提供数据）/ 峰值 / 未释放块。

### 验证（`g++ -std=c++20`，独立编译；按约定未做工程构建）

| 验证项 | 结果 |
|--------|------|
| STL 容器是否进门面 | ✅ `vector`/`string`/`map`/`make_unique` 全部被统计（作用域内 `live=103`） |
| 分配/释放守恒 | ✅ 2000 轮 `new`/`delete` 后 `live=0 owned=0 used=0` |
| 归属判定 | ✅ 门面指针 `owns=1`；栈指针 `owns=0`（**不解引用 ptr，不崩**） |
| 构造/析构配对 | ✅ `Allocate<T>`/`Deallocate<T>` 析构确被调用 |
| `NewFrom<Backend,T>` | ✅ 编译期后端参数，正常构造 |
| 策略路由 | ✅ `setPolicy` 后按 size 路由（Slab 未接入 → 返 nullptr，不崩） |
| 开关 | ✅ 关掉后新分配 `owns=0`（走原生）；释放交门面仍安全 |
| 对齐 | ✅ 满足 `max_align_t`（`Buffer::As<T>` 依赖） |

### 尚未做（下一步）

- **③-b**：`SlabBackend` 迁入（旧 `XMemory.h` 的 slab 实现搬到 `XMemBackend.h`）
- **④**：UI 四层所有权显式化 `Container⊃Layout⊃Dock⊃Panel⊃Component`（`unique_ptr`，dock 按砚台定案区分内建/动态）
- **⑤**：`Shutdown()` 与析构分离（治 `LogViewer::~LogViewer` 里 Join 卡死）
- **⑥**：借用指针断链（`m_MouseCapturePanel` 等）
- **⚠️ 未接入 CMake**：新文件尚未进构建（`XCore` 用 `file(GLOB)`，下次 configure 时自动纳入）
- **⚠️ 多线程**：`OwnedSet` 目前无锁，高频多线程分配需分片或加锁（已记 TODO）

> 审计全文见 `_notes/arch/MemoryAudit.md`。

## 2026-09-13 内存模块拆分 ②：后端 / 统计 / 门面三件套（纯新增，未接入）

> 起因：砚台发现 Widget/UI 里"一堆裸指针但没有清理代码"。审计后确认根因不是漏 delete，
> 而是**所有权从未在代码里落地**；同时决定先把 Memory 模块拆开（本文），再补所有权（后续）。

### 本次范围（只做 ②，不碰 UI、不碰旧 XMemory.h）

| 文件 | 状态 | 职责 |
|------|------|------|
| `XCore/Memory/MemoryBackend.h` | 🏗️ 新 | 后端接口 `IMemoryBackend` + `CrtBackend`（默认后端，标准库兜底） |
| `XCore/Memory/MemoryStats.h` | 🏗️ 新 | 统计独立：`MemoryCounter`（活计数器）+ `MemoryStats`（只读快照） |
| `XCore/Memory/Memory.h` | 🏗️ 新 | 门面：外界唯一入口，分配头 + 后端选择 + 记账 |
| `XCore/Memory/XMemory.h` / `.cpp` | ⏸️ 未动 | 旧 slab 实现原样保留，待迁进 `SlabBackend` |

### 关键设计决定（砚台拍板）

1. **分配 ≠ 所有权**：`Memory` 只管"字节从哪来"，不负责"谁在何时调析构"。
   所有权交给 `unique_ptr`（后续 ③）。上层因此**不负责分配**，只负责构造对象。
2. **默认后端 = CRT（标准库兜底）**：不抢全局堆。自研 slab 只在热点按需启用，
   效果不好随时切回 —— 打消"怕自己写的不如标准库""怕断了别人用标准库的路"两个顾虑。
3. **不做全局 `operator new` 重载**（风险太高）；改由 UI 四层**类作用域**重载（后续 ③/④）。
4. **分配头（`AllocHeader`）方案**：`delete p` 时语言**不传回**构造参数，
   所以"该还给哪个后端"必须记在指针前面。这解决混用：**分配时选后端，释放时不用记**。
5. **门面必须平凡（POD）**：只持指针 + 固定数组，无需分配成员 → 静态初始化期安全，
   不存在构造顺序问题。后端/统计器懒就位，均为零堆分配。
6. **自举铁律**：后端与统计器内部**只用 `::malloc`/`::free`**，绝不回调门面，否则首次
   `allocate` 死循环。
7. **命名贴 STL**（砚台偏好，降低学习成本）：
   `allocate` / `deallocate` / `Allocate<T>` / `Deallocate<T>`；
   旧名 `Alloc` / `Free` / `AllocT` / `FreeT` **保留为 inline 转发**，现有调用点零改动。
8. **统计分维度**：`UsedBytes`（验收泄漏看这个）≠ `Capacity`（池容量，不归零正常）
   ≠ `Overhead`（header/元数据）；且**按后端分开记**。

### 顺带查出的旧 BUG（本次未修，待迁 SlabBackend 时处理）

- 旧 `XMemory.cpp` 的 `Free()`：>64KB 的大块走 `std::free(ptr)` 后
  **没有扣减 `m_UsedBytes` / `m_UsedCapacity`**（`malloc` 指针无法反查大小），
  于是大块分配的**计数只增不减** —— 计数器层面的"泄漏"。
  新门面用 header 记录 size，已从根上解决。

### 验证

按约定琉璃不编译工程，但新文件**未接入构建**，故对三个新头文件做了独立语法/运行自检：

- `g++ -std=c++20 -fsyntax-only` → exit 0
- 运行自检覆盖：旧名兼容、显式后端、`Allocate<T>/Deallocate<T>`（析构确被调用 1 次）、
  对齐满足 `max_align_t`（Buffer 的 `reinterpret_cast<T*>` 依赖它）、
  100 轮分配/释放后 `live=0 / used=0`、门面与 CRT `new` 混用互不干扰、
  误传外部指针被 magic 挡下（不崩、不破坏别的堆）
- 输出确认：`live=0 leaks=0`、`used=0`、`overhead=2640 allocs=165 frees=165`（守恒）

### 下一步（待办）

- **③**：给 UI 四层加类作用域 `operator new/delete`，接到门面上（`static constexpr kBackend` 形态）
- **④**：所有权显式化 `Container⊃Layout⊃Dock⊃Panel⊃Component`（`unique_ptr`），
  按砚台定案：(b) 区分内建 dock / 动态 dock
- **⑤**：`Shutdown()` 与析构分离（治 `LogViewer::~LogViewer` 里 Join 卡死）
- **⑥**：借用指针断链（`m_MouseCapturePanel` 等）
- **⑦**：`SlabBackend` 迁入 + `Buffer` 适配（当前 `Buffer` 走旧名转发即可，无需改）

> 审计全文见 `_notes/arch/MemoryAudit.md`。

## 2026-09-12 — UI 坐标体系统一（修正非顶部 Dock 的 Panel 绘制错位）

> 砚台报的 bug：TabHostContainer 里把 Panel 拖进**除 TopDock 以外的** Dock 会错位，
> 表现是从 (0,0) 起画而不是从 Dock 在窗口内的实际位置起画。TopDock 恰好正常。

### 根因（诊断定案）
四层各持 m_X/m_Y，但**坐标语义不统一**，且绘制链和输入链不对称：

| 层 | 存的坐标 | 参照系 |
|----|---------|--------|
| Dock | 布局绝对 | DockLayout 左上角 |
| Panel | **布局绝对** | 布局左上角（不是 Dock 相对！） |
| Component | Panel 相对 | 所属 Panel 左上角 |
| Canvas | 无坐标系概念 | 绝对 |

- **输入链自洽**：每层 `e.x -= 偏移` 下钻，下钻后还原 → 点击一直是对的。
- **绘制链断裂**：`Panel::OnPaint` 用 `m_X + comp->GetX()` 算 clip（知道要加绝对偏移），
  但传给组件的 canvas **仍是绝对坐标**，组件只得自己再加一次 → `LogViewer.cpp` 里
  手工写 `canvas.FillRect(GetX() + m_ScrollArea->GetX(), ...)` 就是这个病的症状。
- **TopDock 为何正常**：`top=InvalidBoundary, left=InvalidBoundary` → `m_X = m_Y = 0`，
  偏移量恰好为 0，把混用掩盖了。其余四 Dock 偏差量正好是 `Dock::m_X/m_Y`。

### 定案方向（砚台拍板）
砚台确认分层设计：**窗口布局（DockLayout/Dock）用绝对，面板内（Panel/Component）用相对**。
两者各自合理，冲突在于转换点不明确。定案：
- 转换点落在 **Dock↔Panel 交界**，两侧各自纯粹。
- 用 **Canvas 可嵌套 origin 压栈**做转换（不是各组件自己加偏移）。
- 选**选项 2**：origin 在 `Dock::OnPaint` 压，连 Panel 作者都无需感知 ——
  **Panel 内部 + Component 内部全部按 (0,0) 起画**。
- **写法乙**：`DockLayout::OnPaint` 先压 dock origin，使绘制与输入**逐层同构**：
  ```
  绘制: 绝对 ─PushOrigin(dock.X/Y)→ Dock局部 ─PushOrigin(panelX/Y)→ Panel局部
  输入: 绝对 ─减 dock.GetX/Y→        Dock局部 ─减 panelX/Y→         Panel局部
  ```

### 本次改动（第一批：修错位 + 建坐标原语）
- `Widget/Canvas.h`：加 `PushOrigin(dx,dy)` / `PopOrigin()` / `OriginX()` / `OriginY()`（可嵌套压栈）。
- `Widget/CanvasImpl.h`：接口加 `PushOrigin/PopOrigin/GetOriginX/GetOriginY`。
- `Widget/src/Win32/CanvasImplWin32.cpp`：后端持 `m_OriginX/m_OriginY` + 栈；
  `FillRect/FillRoundRect/FillTriangle/FillCircle/SetClip` 全部叠加 origin；
  `Clear/Flush/FlushRect` **不走** origin（物理缓冲操作，无坐标语义）。
  文字走 `Font`（不认原点），故由 `Canvas` 转发壳在传参前加好 `OriginX/Y`。
- 新 `UI/UiCore/UINode.h`：`UINodeView{Rect self, content; bool visible}` + `ContentInParent()`
  + 原语 `ToLocal/ToParent/Hits` + 四层 `View()` 声明（定义在各自 .cpp）。
- **Panel 坐标语义改为 Dock 局部**（`View(Panel).self` 落在父坐标系里）：
  - `Dock::UpdatePanelRects`：改传 `m_EffPanel*`（Dock 局部），不再 `m_X + panelX`。
  - **新增 `m_EffPanelX/Y/W/H` 生效面板区**：声明值 `m_Panel*` 钳到 Dock 内算出生效值，
    绘制原点、输入偏移、命中判定三者统一用它 —— 且钳制结果**不写回声明值**
    （否则窗口缩到 0 再放大会把声明尺寸永久压掉）。
  - `Dock::RouteInput`：删掉 `p->GetX() - m_X` 补丁，改减 `m_EffPanelX/Y`。
  - `Dock::HitTestPanel`：改用 `m_EffPanel*`。
  - `Dock::OnPaint`：Panel 前 `PushOrigin(m_EffPanelX, m_EffPanelY)`；tab 栏改按 Dock 局部 (0,0) 画。
  - `Panel::OnPaint`：clip 去掉 `m_X/m_Y`（origin 已由 Dock 压好）。
- `DockLayout::OnPaint`：遍历 Dock 时 `PushOrigin(dock->GetX(), dock->GetY())`（写法乙）。
- 清理各层手工偏移，统一成"(0,0) 起画"：
  `LogViewer::OnPaint`、`HexViewer::OnPaint` + 内部 BinaryContent、`ScrollArea::OnPaint`
  （改嵌套 origin，滚动位移并入 `PushOrigin(0, -m_ScrollOffset)`）、
  `Horizontal::OnPaint` / `Vertical::OnPaint`（子组件前压 origin）、
  `Label` / `Button` / `TextInput` / `Overlay` / `ListBox`（`x=GetX(),y=GetY()` → `x=0,y=0`）。
- `Dock::HitTestEdge` **保持绝对语义**（由 DockLayout 用未平移坐标调用），加注释钉死，
  防止以后被误"统一"成局部坐标。

### 追加 — 分割线绘制漏用 boundary width（砚台报"压着线绘制"）
- **现象**：分割线看着被 Dock 压扁 / 缝里露出背景色。
- **根因**：`Boundary::width` 是**缝隙总宽**，`Dock::RecalcRect` 确实让出了 `width/2`
  （`left = pos + width/2`、`right = pos - width/2`），但 `DockLayout::OnPaint`
  画线用的是**写死的 `constexpr int thickness = 2`** —— 完全没读 `b.width`。
  于是 `width = 14` 时 Dock 让了 7px 的缝、只画了 2px 的线，剩下 5px 是"真空"露背景。
- **修法**：`DockLayout::OnPaint` 改用 `const int lineW = std::max(1, b.width)`，
  居中 `pos - lineW / 2`。**让多少缝就画多宽**，两边同源。
- 未动 `Dock::RecalcRect`（让缝逻辑本来是对的）、未动 `HitTestEdge`
  （命中厚度已用 `std::max(thickness, b->width / 2)`，本来就没问题）。

### 追加 — 内容溢出容器（滚动时盖住 tab 栏 / 越过边界）：补裁剪三处
> 砚台报：Panel 放进左侧/中间 Dock 后刚放进去就超出底部，往下滚动会盖住 tab 栏；
> 左栏最窄所以最明显（150% DPI）。

- **根因（两层）**：
  1. **Panel 没有外层裁剪**：`Panel::OnPaint` 只对每个组件设 clip，没给整个 Panel 设一次。
     `ScrollArea` 里滚动的内容超出 Panel 矩形就直接画到外面 → 盖住 tab 栏。
  2. **`SetClip` 对文字无效**：文字走 `Font` 直写像素/桥 DC，绕过 `CanvasImpl::BlitPixel`
     的裁剪检查。所以滚动中的日志文字**根本裁不掉** —— 这是溢出的主因。
- **修法（A + B + C2，砚台定）**：
  - **A** `Panel::OnPaint`：加外层 `SetClip(0,0,m_W,m_H)`，画完每个组件后恢复外层裁剪。
  - **B** `Dock::OnPaint`：改为**先画 Panel 内容（带 `SetClip(0,0,m_EffPanelW,m_EffPanelH)`）、
    最后画 tab 栏** —— tab 栏压在最上层，任何内容都盖不住它。
    另 `DockLayout::OnPaint` 给每个 Dock 加自己的裁剪，避免相邻 Dock 互相覆盖。
  - **C2（精确裁剪）**：`CanvasTarget` 加 `clipEnabled/clipX/clipY/clipW/clipH`（物理像素）；
    `CanvasImpl` 加 `GetClipRect()`；`Canvas::MakeTarget()` 把当前裁剪区填进去；
    两个文字后端各自遵守：
      - `FontWin32`（GDI，**当前实际在用**）：`DcClipScope` RAII 对桥 DC 做
        `IntersectClipRect`（TextOutW 自动遵守，抗锯齿边缘也正确），
        `ForceAlpha`/`FillRectIntoBuffer` 走 `InClip()` 逐像素判断。
      - `FontFreeType`：`BlendBitmap`/`FillRectIntoBuffer` 走同一套 clip 判断。
- **影响文件**：`Widget/CanvasImpl.h`、`Widget/Canvas.h`、`Widget/Win32/FontWin32.h`、
  `Widget/Win32/FontFreeType.h`、`Widget/src/Win32/FontWin32.cpp`、
  `Widget/src/Win32/FontFreeType.cpp`、`UI/src/Panel.cpp`、`UI/src/Dock.cpp`、
  `UI/src/DockLayout.cpp`。

### 追加 — 拖进 Dock 后 tab 栏消失（真凶：DockLayout 从未收到尺寸）
> 砚台澄清：**同一个面板在独立窗口(TabContainer)能看到 tab 栏，拖进 Dock(TopLayout) 就看不到**。
> 这说明不是裁剪本身画错，而是 Dock 矩形被压成了 0×0。

- **根因链**：
  1. test 的调用顺序是 `setSize()` → `SetDockLayout()` → `show()`。
     `setSize` 只记逻辑尺寸，窗口要 `show()` 才创建 HWND。
  2. 而 `Container::SetDockLayout` 里 `SetActiveSize` 被包在
     `if (m_Layout && GetNativeHandle())` 内 —— 那一刻 HWND 还是 `nullptr`，
     **`SetActiveSize` 从未被调用** → `DockLayout::m_LayoutW/m_LayoutH` 一直是 **0**。
  3. 此后任何 `RecalcLayout()`（含拖入面板时 `AddPanel` → `Layout->RecalcLayout()`）
     都用 0 尺寸算 → **所有 Dock 的 `m_W/m_H` 被压成 0**。
  4. 于是 `m_EffPanelH = 0`，我上一轮加的 `SetClip(0,0,w,0)` **把整个 Dock 裁没了**，
     tab 栏（`FillRect(0,0,tabW,30)`）也被 `DockLayout` 层的
     `SetClip(0,0,dock->GetWidth(),dock->GetHeight())` = 0 裁掉 → 看不见。
- **修法**：
  1. `Container::SetDockLayout` **无条件**喂尺寸（去掉 `GetNativeHandle()` 条件）。
     `BaseWin::GetActualWidth/Height` 无窗口时返回 `setSize` 存的逻辑值，可直接用；
     窗口 resize 后仍由 `OnWindowResize` 继续同步。
  2. `DockLayout::RecalcLayout` 加守卫：`m_LayoutW/H <= 0` 时直接 return，
     避免真实尺寸到位前把 Dock 压成 0×0（防御性双保险）。
- **涉及文件**：`UI/src/Container.cpp`、`UI/src/DockLayout.cpp`。
- **test 同步清理**：去掉无用的旧拖动模拟残留，补进坐标系/绘制约定注释，
  加 `XY_DEBUG_DOCK_RECTS` 开关（打印各 Dock 矩形 + 面板区，定位布局问题用）。

### 追加 — tab 栏看不见的真正原因：TopLayout 用的是裸 Dock，没有 tab 栏
> 上一节把 `SetActiveSize` 从未被调用修掉后，矩形正常了（砚台贴的打印为证），
> 但 **tab 栏仍然看不见**。用打印数据定位到最终原因。

**打印数据（layout size = 685×662）**：
```
dock#0 (Top)    rect=(0,0 685x130)     panelArea=(0,0 685x130)   panels=0
dock#1 (Bottom) rect=(139,531 407x154) panelArea=(0,0 407x154)   panels=0
dock#2 (Left)   rect=(0,134 135x551)   panelArea=(0,0 135x551)   panels=0
dock#3 (Center) rect=(139,134 407x393) panelArea=(0,0 407x393)   panels=1
dock#4 (Right)  rect=(550,134 135x551) panelArea=(0,0 135x551)   panels=0
```
- **矩形全部正确**：与边界换算吻合（TopLine=132 / BottomLine=529 / LeftLine=137 /
  RightLine=548，各 Dock 让出 `width/2=2` 的缝），**无重叠、无超出**。
  这排除了"整个 Dock 大过预留位置"的猜测。
- **但 `panelArea.y` 全是 0**（带面板的 dock#3 也是 `(0,0 407x393)`），
  本该是 `m_MenuBarHeight = 30` → 说明 **`m_MenuBarHeight == 0`**。

**根因**：`TopLayout` 的五个成员声明为 **`X_Y::Dock`（裸 Dock）**，
而 `SetMenuBarHeight(30)` 是在 **`TabDock` 的构造函数**里做的。
裸 Dock 的 `m_MenuBarHeight` 保持默认 0，于是：
1. `m_PanelY = 0` → 面板区占满整个 Dock 高度（"面板盖住 tab 栏位置 / 超出预留区"）；
2. `Dock::OnPaint` 里 `FillRect(x, 0, tabW, m_MenuBarHeight=0)` → **高度 0，画不出来**。

**这也最终解释了"独立窗口能看到 tab、拖进 Dock 看不到"**：
- 独立窗口走 `TabContainer::CreateSinglePanelDock()` → `new TabDock()`（有 30px 栏）
- `TopLayout` 用的是裸 `Dock`（没有栏）

**修法**：`UI/DockLayout/toplayout.h` 五个成员改为 `X_Y::TabDock`，并加注释说明原因。
**附带效果**：`TabHostContainer::ConfigureTabDocks()` 用 `dynamic_cast<TabDock*>`
挂"拖出成独立窗口"的回调 —— 之前五区域 dock 是裸 Dock，**该回调从未挂上**；现在会正常生效。
- **涉及文件**：`UI/DockLayout/toplayout.h`。

### 追加 — 筛选栏被覆盖：Panel::OnPaint 缺组件级 PushOrigin（坐标统一漏的一环）
> tab 栏修好、裁剪生效后，砚台报 **LogViewer 自己的筛选栏（关键词输入框 / tag 条）看不见了**，
> **悬浮窗口里同样看不到** → 两边都看不到，说明问题在 `LogViewer`/`Panel` 层，与 Dock 无关。

- **根因**：`Panel::OnPaint` 只给每个组件 `SetClip`，**没有 `PushOrigin`**。
  而前面"坐标体系统一"那一批已经把各组件内部全改成按 `(0,0)` 起画
  （`Label`/`Button`/`TextInput`/`ListBox`/`Overlay`/`ScrollArea`…）。
  于是**组件的 `(0,0)` 落在了 Panel 的 `(0,0)`**，而不是组件自己的位置。
- **表现**：`ScrollArea` 被 `OnLayout` 放在 `y=26`，但它的内容画到了 Panel 的 `y=0` ——
  正好盖住上方的 `TagStrip` 和关键词输入框（= 筛选栏）。
- **修法**：`Panel::OnPaint` 对每个组件补齐
  `PushOrigin(comp->GetX(), comp->GetY())` / `PopOrigin()`，与 `Dock` 对 `Panel` 的做法一致；
  裁剪也随之改为组件局部坐标 `SetClip(0, 0, w, h)`。
- **至此"绘制与输入逐层同构"在每一层都成立**：
  `绝对 → Dock局部 → Panel局部 → Component局部`
  （输入链用逐层减偏移，绘制链用逐层压 origin）。
- **涉及文件**：`UI/src/Panel.cpp`。

### 追加 — 交互修复（命中 z 序 / tab 栏穿透 / 落点判定）
> 砚台报：**右侧与底部 Dock 吃不到交互**（滑动无效）；**tab 栏的交互在 Dock 中也吃不到**。

**共 4 处问题，核心是"同一份几何/顺序在多处各写一遍"：**

1. **命中顺序与绘制 z 序相反**（主因）
   - `OnPaint` 正序遍历 `m_Docks`，**后画的在上层**（Right 最上 → Top 最下）。
   - `RouteInput` 也正序遍历、**先到先得**（Top 最先命中）。
   - 结果：视觉上压在最上面的 Dock 反而最难命中。
   - **修法**：新增 `DockLayout::HitTestDock()`，**逆序遍历**，与绘制 z 序对齐。

2. **`m_MouseCaptureDockIndex` 用指针减法当下标**
   ```cpp
   m_MouseCaptureDockIndex = static_cast<std::size_t>(dock - m_Docks.front());
   ```
   依赖"`dock` 确实在 `m_Docks` 里"这一脆弱前提。
   **修法**：改为直接存 `Dock *m_MouseCaptureDock`。

3. **tab 栏事件会穿透到 Panel**
   `Dock::RouteInput` 的 tab 命中只在 `Press` 且命中具体 tab 时设 `Handled`；
   落在 tab 栏**空白处**、或 `Move`/`Release` 时会落到下面的 Panel。
   **修法**：tab 栏条带内的事件**一律吞掉**（UI chrome 区域不穿透）。

4. **tab 宽度三处各写一份 `120`**
   `Dock::OnPaint`、`Dock::RouteInput`、`TabDock::RouteInput` 各一份 → 改一处漏一处就"画的和点的对不上"。
   **修法**：收敛为 `Dock::kTabWidth` + `TabBarTotalWidth()` + `TabIndexAt()`，
   绘制与命中**同源**；`TabDock` 删掉自己的 `kTabWidth`。

**顺带**：`TabDock::DropPanel` 的落点判定原本也是正序遍历，同样改为复用 `HitTestDock` ——
使「**绘制 z 序 / 事件命中 / 拖放落点**」三处顺序彻底统一（一处定义，三处跟随）。

- **涉及文件**：`UI/dock/Dock.h`、`UI/dock/tabdock.h`、`UI/DockLayout/DockLayout.h`、
  `UI/src/Dock.cpp`、`UI/src/DockLayout.cpp`、`UI/src/tabdock.cpp`。

### 追加 — 清理 DockLayout::TakePanel（死代码 + 语义错）
> 砚台确认后删除。`TakePanel(panel, title)` 的五个问题：
> 1. **无人调用** —— UI 重构交接文档预埋的接口，实际落地时走的是 `AddPanelAt`（按落点），从未接线；
> 2. **语义方向错** —— 它找"第一个 `GetActivePanel()` 非空的 Dock"塞进去；
>    该找的恰恰是"**能收容**"的 Dock，有没有激活面板跟能不能收容无关。
>    代码里原有作者批注 `// 这个行为貌似是错的`，核对属实；
> 3. **`new Dock()` 裸 Dock** —— 没有 tab 栏（与刚修的 `TopLayout` 同类问题），且无人 `delete`，是泄漏；
> 4. **正序遍历** —— 又一处没跟绘制 z 序统一的顺序；
> 5. **与 `AddPanelAt` 职责重叠**。
>
> **修法**：删除 `TakePanel`（声明 + 定义），按落点收容统一走 `AddPanelAt`；
> `AddPanelAt` 改为**复用 `HitTestDock`**（逆序 = 上层优先，与绘制/事件命中同一套顺序），
> 并补 `CanAddPanel()` 检查。
> 两个调用方（`tabcontainer` / `tabhostcontainer`）本就是 `if (AddPanelAt(...))` 的用法，
> 返回 `nullptr` 会正常回退到"新建独立窗口"分支，行为安全。
> 另在 `AddPanelAt` 处留注释说明 `TakePanel` 的来龙去脉，避免以后有人从交接文档里又把它捡回来。
- **涉及文件**：`UI/DockLayout/DockLayout.h`、`UI/src/DockLayout.cpp`。

### 追加 — 滚轮在底部/右侧 Dock 失效：WM_MOUSEWHEEL 坐标被重复 DPI 缩放
> 砚台澄清：tab 与分割线交互**都已正常**，问题是"**鼠标在底部/右边的 Dock 面板上滚轮没反应**"。

- **根因**：滚轮坐标被**除了两次 scale**（150% DPI 下多除一次 1.5）。
  1. `Win32WndProc` 的 `WM_MOUSEWHEEL` 分支调了 `pThis->ScreenToClient()`，
     而该接口语义是 **"输入物理, 输出逻辑"**（内部 `÷scale`）；
  2. 但 UI 侧契约是：**所有鼠标 Movement 携带物理客户区坐标**，
     由 `Container` 统一做**唯一一次** `ClientPhysicalToLogical`（又 `÷scale`）。
- **表现**：坐标偏小（往左上偏）→ 原本在**右/下** Dock 的鼠标被算到**左/中** Dock →
  `HitTestDock` 命中错的 Dock → 滚轮事件送错地方。"底部和右边滚不动"即由此而来。
- **修法**：`WM_MOUSEWHEEL` 改用 **`ScreenToClientPhysical`**（物理→物理，不除 scale），
  与 `WM_MOUSEMOVE` / `WM_LBUTTONDOWN` / `WM_LBUTTONUP` 等一致
  （它们都直接传 `lParam` 的原始物理客户区坐标）。
  ⚠️ 注意 `WM_MOUSEWHEEL` 的 `lParam` 是**屏幕**坐标（不同于 `WM_MOUSEMOVE` 的客户区坐标），
  所以仍必须先转客户区，不能直接传 `lParam`。
- **防御**：在 `Container` 构造函数顶部写明**坐标契约**（产出侧必须给物理坐标），
  避免以后又有 Movement 产出侧提前转逻辑坐标导致重复缩放。
- **核查**：其余 `ScreenToClient` 调用（`tabcontainer`/`tabhostcontainer` 跨窗口收养、
  OLE 文件拖放回调）消费方要的**就是逻辑坐标**，一致，无需改动。
- **涉及文件**：`Widget/src/Win32/Win32WndProc.cpp`、`UI/src/Container.cpp`。

### 追加 — 路由/绘制坐标统一落地（UINodeView 契约，不再是预留）
> 砚台指出：前面只**预留**了 `UiCore/UINode.h` 的 `UINodeView`/`ToLocal`/`ToParent`/`Hits`，
> **一处未用**。本次真正接线，四层全部改用。

**统一方式**：C++ 跨类型泛型递归走不通（已确认），所以统一的是「**节点描述 + 坐标原语**」，
不是单一递归函数。每层保留自己的薄路由方法，但：
- 矩形一律取自 `View()`；
- 层间转换一律走 `ToLocal` / `ToParent`；
- 命中判定一律走 `Hits` / `Rect::Contains`。

**各层改动**：
| 位置 | 改动 |
|---|---|
| `DockLayout::HitTestDock` | `Hits(View(*dock))` 判定，逆序（与绘制 z 序一致） |
| `DockLayout::OnPaint` | `View(*dock)` 取 `self` 压 origin/裁剪 |
| `DockLayout::RouteInput` | `ToLocal`/`ToParent` 下钻 Dock |
| `Dock::HitTestPanel` | `View(*this).Content()` 判定 |
| `Dock::OnPaint` | `View(*this).content` 作面板区（压 origin/裁剪） |
| `Dock::RouteInput` | `ToLocal`/`ToParent` 下钻 Panel |
| `Panel::HitTest` | `View(*comp).self.Contains()` |
| `Panel::OnPaint` | `View(*this)` / `View(*comp)` 的 `self`/`content` |
| `Panel::OnInput` | `ToLocal`/`ToParent` 下钻 Component |
| `Horizontal`/`Vertical` | 同上（容器型 Component 也纳入） |

**关键点**：`View(Dock).content` **直接引用 `m_EffPanel*`**（生效面板区），
而不是用 `m_MenuBarHeight` 另算一份 —— 避免又引入第二份"面板区真相"
（过去 `m_Panel*` / `m_EffPanel*` / `Panel::GetX()` 三份数据打架正是根因）。
新增 `Dock::GetEffPanelX/Y/W/H` 访问器供 `View` 使用。

**结果**：UI 模块内**手写坐标算术全部清零**（`grep "e.x -=|e.x +="` 只剩注释）。
唯一保留的特例是 `ScrollArea::PushOrigin(0, -m_ScrollOffset)` —— 那是滚动**变换**而非节点位置，语义不同。

- **涉及文件**：`UI/UiCore/UINode.h`、`UI/dock/Dock.h`、`UI/src/Dock.cpp`、
  `UI/src/DockLayout.cpp`、`UI/src/Panel.cpp`、`UI/src/horizontal.cpp`、`UI/src/vertical.cpp`。

### 追加 — 修复回归：HitTestDock 误用 content 导致 tab 拖不动
> 坐标统一落地后砚台立刻报"**tab 又不能拖动了**"。是我引入的回归。

- **根因**：`HitTestDock` 里写了 `Hits(View(*dock))`，而 `Hits` 判的是 **`content`（内容区）**；
  `View(Dock).content` = `m_EffPanel*`，**从 tab 栏下方开始**。
  于是鼠标按在 tab 栏上时 `HitTestDock` 返回 `nullptr` → `m_MouseCaptureDock` 拿不到
  → 事件根本下不去 → `TabDock` 收不到 Press → **tab 拖不动**。
- **语义辨析（关键）**：「命中哪个 Dock」与「命中 Dock 内的面板内容」是两件事：
  | 用途 | 用哪个矩形 | 位置 |
  |---|---|---|
  | 找**节点本身**（把事件交给它） | **`self`**（Dock 全矩形，含 tab 栏） | `DockLayout::HitTestDock` |
  | 找**节点内的内容区** | **`content`**（避开 tab 栏） | `Dock::HitTestPanel` |
- **修法**：`HitTestDock` 改用 `View(*dock).self.Contains(x, y)`。
- **附带加固**：**从 `UINode.h` 移除 `Hits()` 原语**。它的名字暗示"命中"却静默选了
  `content` 而非 `self`，正是本次事故的成因；移除后已无调用方。
  改为要求调用方**显式写** `self.Contains(...)` / `Content().Contains(...)`，
  读代码时一眼能看出用的是哪个矩形。原语收敛为 `ToLocal` / `ToParent` 两个。
- **全量审计**：所有 `.self` / `.content` 使用点已逐处核对（见 DEVLOG 下方表格）。
  **`Dock` 是唯一 `self != content` 的节点**，它正确地在"找节点本身"处用 `self`
  （`HitTestDock` / 压 origin / 下钻）、在"找面板内容区"处用 `content`
  （`HitTestPanel` / 画 Panel 前的 origin）。`Panel`/`Component` 的 `self == content`，两可。
- **涉及文件**：`UI/UiCore/UINode.h`、`UI/src/DockLayout.cpp`、`UI/src/Panel.cpp`。

### 已知问题（下一轮）
- `Dock` 的 tab 栏仍不绘制标题文字（只画色块），宽度固定 `kTabWidth = 120`，
  未按标题测宽。字体绘制需接 `FontLibrary`。

### 已知限制（本次未处理，非本次引入）
- `Canvas::SetClip` 只对**图形**（`BlitPixel` 路径）生效；**文字**经 `Font` 直写像素/桥 DC，
  不走裁剪。即超出 clip 的文字仍会画出来。属既有行为，滚动区文字可能溢出。
- `Dock::OnPaint` 的 tab 栏仍不画标题文字（只画色块），宽度硬编码 120。

### 待办（第二批：统一驱动器，本次不做）
- 把四层 `OnPaint`/`RouteInput` 重构成 `PaintSelf`/`PaintChildren` + 递归驱动器。
- SpecialLayer 优先级显式化（boundary > tab 栏 > 子节点树；z 序 = 路由优先级）。
- 分割线/边界命中与绘制共用 `UINodeView`。

### 顺延
- **窗口资源释放/泄露**问题（砚台：先不管，本次不动）。
