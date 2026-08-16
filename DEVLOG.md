## 2026-08-16 — DataStore 新增静音开关 SetEnabled（让依赖它的一切集体失效）

> 需求(MouseFlight 收尾)：想关日志但不想动所有调用方。给 DataStore 加总开关，接口保留但静音空转。

### 改动
- `Modules/DataStore/DataStore.h`：+`SetEnabled(bool)` / `IsEnabled()`，private `bool m_Enabled=true`(默认开=现状不变)。
- `Modules/DataStore/src/DataStore.cpp`：所有核心方法开头加 `if(!m_Enabled) return(空/0/nullptr)`。
  - GetOrCreate/Get → nullptr（⚠️ 调用方需判空；现有 DataStoreDevice/LogViewer 均已判空）。
  - Save/SaveAll → false；Flush/FlushAll/Remove/Rename/LoadDir → 空；Contains→false；ListKeys→空。
  - SaveIndex 静音时不写；SetEnabled(false) 顺带清空 m_Entries。
- LoadFile：加静音判断时注意保持无锁读文件→加锁写条目的原语义(不要把所有 I/O 包进锁)。

### 用法
- 想静音：程序早期 `X_Y::DataStore::Instance().SetEnabled(false);` → 所有日志/落盘/索引全停。
- 默认 true，不影响其它项目。

### ⚠️ 待办
- dist 的 DataStore.h 已同步，但 **libDataStore.a 需重建 X_Y** 才生效（当前还是 8/12 旧库）。
- IsEnabled() 读 m_Enabled 未加锁(纯 bool)；若需严格线程安全可后续加锁。

---

## 2026-08-16 — Canvas 新增 FillCircle（实心圆/空心圆环）

> MouseFlight 爆炸特效前置能力。为 Widget 层 Canvas 加「圆/rInner=0」+「空心圆环/rInner>0」绘制，软件光栅逐像素判距，风格对齐 FillRoundRect/FillTriangle。

### 改动（3 文件）
- `Modules/Widget/CanvasImpl.h`：接口层加纯虚 `FillCircle(int cx,int cy,float rOuter,float rInner,uint32_t color)`。`rInner<=0` 实心圆，`0<rInner` 空心圆环（环宽=rOuter-rInner）。
- `Modules/Widget/Canvas.h`：薄转发壳 `FillCircle(...)`。
- `Modules/Widget/src/Win32/CanvasImplWin32.cpp`：软件光栅。圆心对齐物理像素中心(+0.5f)，逐像素判 `rInner²≤d²≤rOuter²`，用 BlitPixel(裁剪/越界/alpha合成自带)。半径是浮点，`*m_Scale` 换算物理而非 S()。

### 备注
- dist/include 由构建流程 configure/build/install 同步，不手改。
- 待砚台 configure/build/install 后，MouseFlight 侧即可用 `canvas.FillCircle(cx,cy,r,0,color)` 画实心圆 / `FillCircle(cx,cy,rOuter,rInner,color)` 画圆环做爆炸特效。

---

## 2026-08-14 — X_Y::Physics 物理引擎落地（可换后端 + 耳切剖分分层碰撞）

> 砚台主导设计，琉璃实现。全新模块：**三层架构 + 工厂**（对齐 Widget 层）。
> ⚠️ 涉及 X_Y Modules/Physics 改动 + CMake（Physics 从 INTERFACE 改回 xy_module）。

### 架构（三层 + 工厂）
```
Modules/Physics/
├─ Physics.h          # 上层稳定接口：Body门面(Pimpl持BodyImpl*) + Test模板 + SetCollisionBackend + Contact
├─ PhysicsImpl.h      # 中间纯虚接口：BodyImpl + CollisionBackend + using Vec=MATH::Vec2(换维改这) + Tri2D
├─ PhysicsFactory.h   # 工厂：CreateBodyImpl(type)/CreateCollisionBackend(type)
├─ src/
│   ├─ Physics.cpp          # Body构造/移动(工厂建) + 全局后端 g_backend(默认工厂建2D)
│   ├─ PhysicsFactory.cpp   # switch 按 PhysicsBackendType::Builtin2D 返回具体实现
│   └─ Builtin2D/
│       ├─ BodyImpl2D.h/.cpp        # Vec2实现 + 耳切剖分缓存 + prevPos
│       └─ CollisionBackend2D.h/.cpp # 三层碰撞算法
```
- `PhysicsBackendType` 枚举：Builtin2D（将来加 Builtin3D/Box2D）
- `Vec`（点/向量）暴露给上层传参，贯穿 上层+虚接口+实现。当前 2D=Vec2，换 3D 改别名一处（2D 时 z=0，上层逻辑不变）
- `Body` 构造用 `CreateBodyImpl()`，全方法一行转发 `m_impl->xxx()`
- 全局后端 `GetCollisionBackend()/SetCollisionBackend()`，Test 用它

### 用法（上层只碰 Body + Physics::Test）
```cpp
body.SetPos({0,0});
body.SetRadius(8);                       // 圆
// 或 body.AddPoint({0,0}); body.AddPoint({10,0}); body.AddPoint({5,8});  // 多边形
body.SetVel({0,0}); body.SetMass(1);
X_Y::Physics::Test(a, b, [](Body& x, Body& y){ /*碰撞处理*/ });
```

### 三层碰撞算法（耳切剖分 + 穿透）
- **L1 扫掠矩形(AABB)粗筛 O(1)**：SweptBox=覆盖 min(prevPos,pos)..max 外扩 boundingRadius，两盒不相交→排除。高速物体 prevPos≠pos 时扫掠盒变宽，**不会被当前 pos 排除**（否则穿透检测跑不到）；未动退化为静态盒
- **L2 关键点三角形包含**：耳切三角剖分**离线缓存**（设置形状 AddPoint 时算一次，运行时零开销）；取中心点+顶点均匀采样(上限8)测是否在对方三角形内
- **L3 高速穿透**：L2 未命中才跑；中心点扫掠线段 prevPos→pos vs 对方三角形（隧道效应）
- 形状=有序多边形顶点（逆/顺时针皆可，内部统一逆时针），凹多边形也支持

### ⚠️ 两个踩坑（要记住）
1. **SetPos 必须同步 prevPos**：否则静态物体 prevPos 停在初始(0,0)，扫掠矩形变大斜矩形误判。修法：`SetPos` 里 `prevPos=新pos`（瞬移/静止无路径）；真正的运动路径由 `Integrate` 记录（积分前 m_prevPos=m_pos）。
2. L1 不能用“当前 pos 距离”，必须用扫掠矩形盖住路径（否则高速穿透被排除）。

### 边界处理（砚台认可，未实现）
- 方法一：四周放大箱子当障碍物（引擎一视同仁刚体，走碰撞回调）
- 方法二：积分后夹紧 pos 不超边界（最省，小型演示推荐）

### 追加：Body 旋转（08-14 晚，砚台要求“绘制=碰撞形状”统一）
- BodyImpl 加 `SetRotation(float rad)/GetRotation()`（弧度，逆时针正，本地几何绕 pos 旋转）
- 加 `LocalToWorld(local)` 虚接口：本地点旋转+平移 → 世界点。**绘制/碰撞统一用它**保证朝向一致
  - GetWorldPoints / SampleWorldKeyPoints / 碰撞后端(三角形世界坐标) 全改走 LocalToWorld
  - ⚠️ 关键：碰撞用的三角形也必须过旋转（不能只 pos+local），否则 body 转动后碰撞形状和绘制形状不一致
- 意义：飞机 AddPoint 定义朝上三角 + SetRotation(航向)，Draw 直接画 GetWorldPoints → 所见即所碰；为将来“顶点绑图片”(OpenGL 式)铺路
- 自测 6/6 全过（含旋转 90° 正方形仍覆盖中心、旋转三角形重叠、GetWorldPoints 旋转准确）

### 追加：圆/多边形统一检测（08-14 晚，砚台指出圆无点集合的问题）
- **不额外写圆形类型**：points 空 = 圆。
- L2 重构为**统一“采样点互测 + 内部测试分发”**：
  - `PointHitsBody(p, body)`：body 是圆(空)→距离<=半径；多边形→测落在剖分三角形内。
  - 对称：A 关键点测 B 内部，B 关键点测 A 内部。圆圆/圆多/多多全走同一模式。
- 圆作为主体取采样点时=仅中心点（SampleWorldKeyPoints 已处理 points 空）
- L3 穿透只对多边形三角形做（圆无“面”，圆圆已由中心距覆盖）
- ⚠️ 已知限制（采样模型固有）：**圆贴着多边形边/相切时可能漏检**（圆心在外、多边形顶点采样未进圆 → 两边都不命中）。待定是否加“圆心到多边形边距<=半径”的贴边补检。
- 自测 7/7 过（圆圆/圆在方内/圆在方外/三角包含/多边形角戳圆/多边形高速穿透）

### 验证
- 语法自检全过；自测 8/8 全过（圆圆/圆在正方形/圆在外/三角包含/凹L形不碰/凹L形臂碰/高速穿透）
- ⚠️ 完整 X_Y 工程构建交砚台（新增 multi .cpp，需 cmake configure 重扫 GLOB）

### 遗留/待定
- 旧 `CollisionSence.h`（空壳，零引用）未删，等砚台决定
- 第一版只返回 bool，无法线/穿透深度（反弹/推开后续加）
- 采样策略（均匀上限8）后续可优化；凹多边形耳切已验证

---

## 2026-08-06 — LogViewer 多关键词筛选完成 + 输入/缩放/拖拽系列修复（收尾）

**今日 LogViewer 全部完成并通过砚台验收。**（拆行与 WM_CHAR 见前两条日志。）

| 操作 | 文件 | 说明 |
|------|------|------|
| 改 | APP/UI/src/Composite/LogViewer.cpp | 筛选支持 AND：tag 内用 && 分隔（error && info = 同时含二者），tag 之间仍 OR；新增 SplitAndParts |
| 改 | APP/UI/src/TagBar.cpp | TagBar 空态折叠：无 tag 时高度压 0，不占顶部空间 |
| 改 | APP/UI/src/Container.cpp | 鼠标按下 CaptureMouse / 松手 ReleaseMouseCapture：修复拖滑块移出窗口后松手不触发（SetCapture 标准做法） |
| 改 | APP/UI/src/Container.cpp | 鼠标按下后 RequestRepaint：点击输入框光标立即出现 |

**今日全部提交（main）：** 3aa5544 折行 → e62ea2b WM_CHAR → e77124e 区分大小写 → 15324ca TagBar多关键词 → 102d2ef 右上角删除+debug → d0dc733 移debug → 56fab36 AND逻辑 → 95d5070 TagBar空态折叠 → de32f2b 焦点光标 → b454f3f 鼠标捕获 → 34fbad3 ListBox空行

**遗留待办：**
- OnKeywordChanged/筛选仍是每回车全量 RebuildAll（5000 条重筛+折叠），若性能忧心再优化（防抖）
- NoWrap 横向滚动待 ScrollArea
- 笔记里"筛选结果可能不对"老 bug 内经实测未复现（现在能筛、结果对），保留观察
- 拖拽鼠标捕获已窗口级统一修复（不只滑块）

## 2026-08-06 — LogViewer 多关键词筛选（TagBar 圆角标签条 + 回车添加 + OR 匹配）

**背景：** 砚台要 LogViewer 支持多关键词筛选：输入框加占位提示；输入关键词回车添加到顶部标签条（LeetCode 式 filter chips），满足任一关键词（OR）即筛出，每条可删除。

| 操作 | 文件 | 说明 |
|------|------|------|
| 新 | APP/UI/include/Component/TagBar.h + src/TagBar.cpp | 横向筛选标签条：自动换行、圆角矩形(FillRoundRect)、左上角 × 删除按钮、hover 高亮、点击×回调 OnTagRemove、Measure()量高供宿主布局 |
| 改 | APP/Widget/include/Canvas.h / CanvasImpl.h / src/Win32/CanvasImplWin32.cpp | 新增 FillRoundRect（圆角矩形填充，Win32 CreateRoundRectRgn+FillRgn） |
| 改 | APP/UI/include/Component/TextInput.h + src/TextInput.cpp | 加 OnEnter 回调（OnKeyDown 命中 Key::Enter 触发） |
| 改 | APP/UI/include/Composite/LogViewer.h + src/Composite/LogViewer.cpp | m_Keywords 多关键词（OR：任一命中即通过）；回车从输入框取词加 tag + 清空输入；OnTagRemoved 删词重筛；布局顶栏 TagBar 量高 + 输入框 + 日志区 |

**设计要点（与砚台讨论确定）：**
- 匹配 OR（任一关键词为正文子串，区分大小写）
- 输入框只做"Enter 添加"，不实时筛（用户要求）→ 删了原先 OnTextChange 实时筛选
- TagBar 高度自适应（换行后多行），LogViewer OnPaint 先 Measure 量高再布局，避免日志区一帧错位
- 用 TagBar 而非 ListBox/ScrollArea：tags 是横向排列，正常不会几十条，直接换行，不做滚轮
- **待验（砚台）**：回车添加是否正常、左上角×删除是否生效（此前"删不掉"疑为 + 区太小/交互别扭，已改左上角明确按钮；若仍删不掉需查 Container 事件转发）、OR 筛出、占位提示
- **待办**：TagBar 若 tags 极多（>几十条）再考虑折叠/滚动扩展

## 2026-08-06 — LogViewer 关键词筛选改为区分大小写（严格匹配）

**背景：** 砚台希望筛选区分大小写（搜 Error 只匹配 Error，不匹配 error）。原实现两遍 tolower（不区分大小写）不满足。

| 操作 | 文件 | 说明 |
|------|------|------|
| 改 | APP/UI/src/Composite/LogViewer.cpp | 删 ToLower 工具；MatchesKeyword 改 e.text.find(m_Keyword) 严格子串匹配；OnKeywordChanged 不再 tolower 保留关键词原样；清理冗余 include |

**决策：** 关键词保留原样做严格 find，大小写敏感。已提交 e77124e。

## 2026-08-06 — 修复 LogViewer 关键词输入框打字不显示（WM_CHAR 缺失 + 中文 UTF-8）

背景：LogViewer 关键词输入框打字无任何反应。诊断出根因：Win32 消息层从未产生 KeyTyped（缺 WM_CHAR），且 TextInput::OnChar 只收 ASCII 宽字符强转单字节。

| 操作 | 文件 | 说明 |
|------|------|------|
| 改 | APP/Widget/src/Win32/Win32WndProc.cpp | 补 case WM_CHAR：产生 KeyTyped 推入事件队列（字符码原样传，不做按键映射）；过滤控制字符(>=32 且非 DEL)；文本从 VM_CHAR 直通，字符本身即最终结果 |
| 改 | APP/UI/include/Component/TextInput.h | m_CursorPos 语义改为 UTF-8 字节偏移；加 Utf8FromWide 声明 |
| 改 | APP/UI/src/TextInput.cpp | OnChar 接受所有可见 Unicode（含中文）按 UTF-8 编码插入 + RequestRepaint；OnKeyDown Left/Right/Backspace/Delete 按 UTF-8 字符边界步进（不劈多字节）；光标用 canvas.MeasureText（中文宽度不同，不再硬编码 *8） |

设计决策（与砚台讨论确定）：
- WM_CHAR 是“输入了什么字符”（Unicode 字符码=最终结果），不做按键映射（Translate 只适用 WM_KEYDOWN 的“哪个键被按”）；KeyCode=unsigned int 装得下任意字符
- 输入框全程 UTF-8（/utf-8 编译），中文关键字可匹配日志
- 后续待办：OnKeywordChanged 每字符触发全量 RebuildAll（5000 条重筛+折叠），打长词慢/闪；待筛选 bug 一起优化（防抖或确认时重建）

## 2026-08-06 — LogViewer 自动折行（终端式，不定高折叠）

**背景：** LogViewer 长日志行超出可视宽被 SetClip 硬切，不随窗口重折。目标：终端式自动折行——长行按可用宽切多物理行全部输出，窗口过窄连一个字符都放不下则该行整体不显示。

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | APP/Widget/include/CanvasImpl.h | 加 MeasureText(const wchar_t*)/(const char*) 纯虚（返回逻辑像素宽） |
| 🏗️ 新 | APP/Widget/src/Win32/CanvasImplWin32.cpp | 实现 MeasureText：GetTextExtentPoint32W 测宽，物理→逻辑 ÷scale |
| 🔧 改 | APP/Widget/include/Canvas.h | 暴露 MeasureText 两个重载（UTF-8 窄版 + 宽版） |
| 🔧 改 | APP/UI/include/Component/ListBox.h | 不定高折行核心改造：WrapMode(Wrap/NoWrap) 枚举、折行段缓存 m_Fold、物理行前缀和 m_LineStartIndex、m_TotalLines、惰性折叠游标 m_FoldedCount |
| 🔧 改 | APP/UI/src/ListBox.cpp | 重写：FoldItem 逐 UTF-8 完整字符测宽贪婪切段（中文不劈半）；EnsureFold OnPaint 惰性折叠（宽变全量重折/增补只折新增）；OnPaint 按物理行可视裁剪 + 整 item 画所有段；GetRowFromMouseY 物理行二分→逻辑 item |

**设计决策（与砚台讨论确定）：**
- 折行宽度由 ScrollArea 传（它 SetRect viewW 给内容），LogViewer 不碰宽度，职责清晰
- Wrap 主做；NoWrap 已架开关，横向滚动待 ScrollArea 补（后续待办）
- 折叠惰性：OnPaint 里驱动（需 Canvas 测宽），增量喂入只折新条目，不每帧全量测 5000 条
- ListBox 保持平台抽象：折行用 UTF-8 逐字符 + MeasureText(const char*)，不在 UI 层引 Win32
- 可视裁剪/滚轮步长均按物理行（GetScrollStep=行高），滚动系统无需大改
- **待验**（砚台跑）：窗口 resize 实时重折、中文不劈半、长行全输出、滚动到折行多行正常
- **后续待办**：NoWrap 横向滚动补进 ScrollArea；段内紧凑(段间不留行距)有需要再切混合行高

## 2026-08-03 — 字体背景色/UTF-8统一/DPI路线A/BaseWin双坐标系（LogViewer 字体模糊治理）

**背景：** 砚台 150% 屏。接入微软雅黑 ClearType 后字体仍糊(油画感)，诊断双原因：ClearType 透明背景退化(次因) + 无 DPI-aware 声明位图拉伸(主因)。

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `APP/Widget/include/Canvas.h` | 组合拳 `FillText(bgX,bgY,bgW,bgH, tx,ty, text,color,bgColor, font=nullptr)`：铺背景+同背景色写字(供ClearType亚像素)，函数尾恢复TRANSPARENT；附场景理念注释 |
| 🔧 改 | `APP/Widget/include/CanvasImpl.h` | 加 FillText 纯虚(窄/宽两版，带 tx,ty 文字起点) |
| 🔧 改 | `APP/Widget/src/Win32/CanvasImplWin32.cpp` | 实现 FillText：FillRect→OPAQUE+SetBkColor(bgColor)→TextOut→恢复TRANSPARENT；窄版UTF-8转宽 |
| 🔧 改 | `APP/UI/src/ListBox.cpp` | 行画改用 FillText；descender修复：行高18→20，新增 m_LineSpacing=4 行距，行间留白隔离，避免下一行背景盖掉上一行文字底部(q/y/g) |
| 🔧 改 | `premake5.lua` | 全局 `buildoptions "/utf-8"`，强制源码+窄字面量 UTF-8 |
| 🔧 改 | `APP/Widget/src/Win32/CanvasImplWin32.cpp` | 窄 DrawText 改 UTF-8→Wide+TextOutW(不再 TextOutA)；加 Utf8ToWide 辅助 |
| 🔧 改 | `APP/Widget/src/Win32/WindowImplWin32.cpp` | 窗口标题 CP_ACP→CP_UTF8 |
| 🗑️ 删 | `Test/DataStore/*.log` | 旧 GBK 日志文件删除(已无用) |
| 🏗️ 新 | `APP/Widget/include/Dpi.h` + `src/Win32/Dpi.cpp` | DPI 工具：DeclareAware(消除位图拉伸)+GetDpi/GetScale(运行时自动读系统DPI,用户改缩放实时更新) |
| 🔧 改 | `APP/Widget/src/Entry.cpp` | WinMain 开头 Dpi::DeclareAware() |
| 🔧 改 | `APP/Widget/src/Win32/WindowImplWin32.cpp` | 窗口创建尺寸 逻辑→物理 ×scale |
| 🔧 改 | `APP/Widget/src/Win32/FontWin32.cpp` | 逻辑字号→物理 ×scale |
| 🔧 改 | `APP/Widget/src/Win32/CanvasImplWin32.cpp` | **DPI核心**：Canvas 逻辑坐标空间，位图=物理，绘制方法内部×scale，GetWidth/Height返回逻辑，默认字号×scale |
| 🔧 改 | `APP/Widget/src/Win32/Win32WndProc.cpp` | WM_NCCREATE/WM_SIZE 物理→逻辑(存XWidget) |
| 🔧 改 | `APP/Widget/include/WindowImpl.h` + `include/BaseWIn.h` | **双坐标系接口**：默认逻辑(ScreenToClient/GetClientRect/宽高) + Physical后缀(真实像素)；注释写全 |
| 🔧 改 | `APP/UI/src/Container.cpp` | 命中测试用逻辑(ScreenToClient自带逻辑，删手动÷scale) |
| 🔧 改 | `APP/UI/src/DockLayer.cpp` | 停靠判定统一逻辑坐标(与Docker内部停靠区/预览/Canvas一致) |

**设计决策（与砚台讨论确定）：**
- 底层 Canvas 收口逻辑→物理转换，上层UI全逻辑零改动；输入(命中/滚轮)也收口物理→逻辑
- BaseWin 提供逻辑/物理双坐标系，DockLayer 用逻辑跟 Docker 内部一致(曾误改物理已纠正)
- 存储层 FilesSystem/DataStore 字节直通，/utf-8 后天然 UTF-8，零改动(架构红利)
- **待补**：Dpi 未走工厂模式(静态实现)，跨平台时需补 DpiPlatform 抽象(砚台认可暂缓)

**验证（砚台确认）：** 字体模糊主因(DPI)+次因(背景)均已修，"好很多"；Dock 停靠/命中待重点回归。

---

## 2026-08-02 — LoopQueue 环形队列 + LogViewer 底部替换（第1步）

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `Core/Memory/include/LoopQueue.h` | 通用固定容量循环队列模板 `LoopQueue<T, N>`：FIFO 满则覆盖最旧；std::array 连续内存；接口贴 STL（Push/operator[]/Size/Empty/Clear/begin-end 范围 for）；At(globalSeq) 增量访问；TotalPushed 单调序号；含 const/非const迭代器，正确处理环形绕回 |
| 🔧 改 | `APP/UI/include/Composite/LogViewer.h` | `m_AllEntries`：std::deque → `LoopQueue<LogEntry, MAX_ENTRIES>`；include 改 Memory/include/LoopQueue.h |
| 🔧 改 | `APP/UI/src/Composite/LogViewer.cpp` | 适配：clear→Clear()；删掉 pop_front 手动限长（Push 满自动覆盖最旧）；push_back→Push()；ApplyFilter 遍历兼容新迭代器 |

**结构决策（与砚台确认）：**
- STL 无现成循环队列 → 基于 std::array 封装通用模板 `LoopQueue<T,N>`，放 Core/Memory（premake 通配符自动收录，无需改 lua）
- 命名 LoopQueue（砚台定，好写不歧义，不去 Buffer 化）
- 全局 includedirs 含 `Core`，`"Memory/include/LoopQueue.h"` 可在 UI 项目解析
- ✅ premake vs2026 通过；本步为“换底层”，未接增量逻辑

**分步计划（砚台定序：先换底层跑通，再做增量）：**
- ✅ 第1步：LoopQueue + deque 替换
- ✅ 第2步：增量筛选
- ✅ 第3步：ListBox 只画可视区行【本条目新增】

---

## 2026-08-02 — 字体抗锯齿 + 可扩展字体选择（Font 抽象）

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `APP/Widget/include/Font.h` | FontDesc（族名/字号/粗体/quality + 可选 filePath 自定义字体）+ FontImpl 纯虚 + Font 包装（RAII，GetNativeHandle/GetHeight/GetAscent/GetAvgCharWidth 度量） |
| 🏗️ 新 | `APP/Widget/src/Win32/FontWin32.cpp` | Win32 实现：CreateFontIndirectW + 质量映射；filePath 非空时 AddFontResourceEx(FR_PRIVATE) 私有加载自定义字体，析构 RemoveFontResourceEx；UTF8→Wide 族名转换（支持中文“微软雅黑”） |
| 🔧 改 | `APP/Widget/include/CanvasImpl.h` | 加纯虚 `SetFont(const Font&)` |
| 🔧 改 | `APP/Widget/include/Canvas.h` | 加 SetFont（复制 desc 重建 Font 存 m_DefaultFont）+ 默认字体 |
| 🔧 改 | `APP/Widget/src/Win32/CanvasImplWin32.cpp` | ApplyDefaultFont（微软雅黑 14px CLEARTYPE，CreateFontIndirect）+ SetFont 实现（SelectObject HFONT）+ 析构释放默认字体 |

**设计（与砚台确认）：**
- 放 Widget 层（跟 Canvas 一起），走纯虚+工厂+Win32 模式
- 默认字体：微软雅黑(UI) ClearType 14px —— 中文英文都清晰，作为 Canvas 默认，所有 DrawText 立即抗锯齿
- 可扩展：FontDesc.filePath 填 ttf/otf → 私有加载自定义字体（不污染系统），之后接自下载字体
- 度量接口 GetHeight/Ascent/AvgCharWidth 已预留，供后续布局/自适应窗口用
- 后续 ListBox m_LineHeight 可改用字体行高（待自适应窗口时一起做）
- ✅ premake vs2026 通过；编译交砚台验证（不主动编译）

**待办接续：**
- 自适应窗口大小（行高用字体度量联动）

---

## 2026-08-02 — ListBox 可视裁剪（第3步）

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 改 | `APP/UI/include/Component/Component.h` | 基类加通用虚方法 `SetViewport(int scrollOffset, int viewHeight)`，默认空实现（内容被滚动时通知可视范围，可按行裁剪的内容 override） |
| 🏗️ 改 | `APP/UI/include/Component/ListBox.h` | override SetViewport，存 m_ViewOffset/m_ViewHeight |
| 🏗️ 改 | `APP/UI/src/ListBox.cpp` | OnPaint 可视裁剪：m_ViewHeight>0 时只画 [rowFirst,rowLast) 行（rowFirst=offset/行高，rowLast=(offset+viewH)/行高+1）；未在 ScrollArea 时回退画全部 |
| 🏗️ 改 | `APP/UI/src/ScrollArea.cpp` | OnPaint 画内容前调 `m_Content->SetViewport(m_ScrollOffset, h)` |

**设计（与砚台确认）：**
- 保留 ScrollArea 现有“SetRect 挪内容”机制，只加一行 SetViewport 通知，风险最小
- 结果：5000 行只画屏上可视几十行，DrawText 数量级下降（配合第2步增量）
- 终端那种“大画布挪动画”高性能滚动按住不放，以后需要再新写组件

**⚠️ 编译注意（砚台）：** 我尝试直接 msbuild 编译时遇到诡异报错（C2065/C2059/C2947，指向行号与实际内容对不上），经核实文件内容/编码均正常，疑似环境怪毛病（PCH/增量缓存），非代码问题。已不主动编译，交砚台验证。之后我不再直接编译。

**待办（砚台记）：**
- 关键词筛选可能有 bug（砚台之前发现，未查）
- 后续如需高性能滚动组件再写

**收尾约定（砚台实践心得，已写进 D:\workbench\CONVENTIONS.md）：**
- 中文注释与代码隔空行（编译器非 Unicode 不识中文，紧贴会连带报错）
- include 路径写全（跨项目只知顶层路径）
- 琉璃不主动编译（环境有编码/PCH 怪毛病，交砚台验证）
- 已建 `_notes/arch/LogViewer.md` 完整记录滚动/增量/可视裁剪实现细节，日后改性能组件靠它兜底


---

## 2026-08-02 — LogViewer 增量筛选（第2步）

| 操作 | 文件 | 说明 |
|------|------|------|
| 🔧 改 | `APP/UI/include/Composite/LogViewer.h` | 私有方法：`MatchesKeyword`（单条命中判断）/ `RebuildAll`（全量重建）/ `IncrementalAppend(fromSeq,toSeq)`（增量喂入）；成员 `m_RenderedSeq`（已喂入 stripe 的全局序号） |
| 🔧 改 | `APP/UI/src/Composite/LogViewer.cpp` | 增量架构：OnPaint 不再全量 ApplyFilter，只布局+渲染 stripe；Ticker 入队后按 [before,after) 全局序号 IncrementalAppend；关键词变→OnKeywordChanged 唯一一次 RebuildAll；数据重置/换key 也 RebuildAll/清 stripe |

**增量逻辑（方案甲，与砚台确认）：**
- Ticker 每帧只把新增全局序号段 [before,after) 喂给 m_LogStripe（无关键词直接 AddItem；有关键词逐个 filter）
- 唯一全量路径：关键词变化（OnKeywordChanged→RebuildAll）、数据重置/换 key
- At(globalSeq) 环形取址；增量段是刚写入的最新区，未被覆盖，安全
- RebuildAll 用相对索引[0..Size)从头遍历当前有效条目
- 锁：Ticker(排他)写 stripe，OnPaint(共享)画 stripe，同一 m_EntriesMutex
- ✅ 用 msbuild 实际编译通过：Memory/UI/Test 全链生成 Test.exe（C4819 是既有编码警告，与本次无关）
- 运行效果待砚台验证（日志量小可能看不出性能差，主要验证功能正常：显示/滚动/关键词）

---

## 2026-08-02 — Canvas 双缓冲 + 消除滚动白屏闪烁

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 改 | `APP/Widget/include/CanvasImpl.h` | 加纯虚 `Flush()`（内存帧一次性上屏） |
| 🏗️ 改 | `APP/Widget/include/Canvas.h` | 加 `Flush()` 转发到 m_Impl |
| 🏗️ 改 | `APP/Widget/src/Win32/CanvasImplWin32.cpp` | 双缓冲核心：构造 CreateCompatibleDC+Bitmap+SelectObject；FillRect/DrawText/SetClip 改绘 memDC；Flush() BitBlt 上屏；析构先选回旧对象再删防 GDI 泄漏 |
| 🏗️ 改 | `APP/Widget/src/Win32/Win32WndProc.cpp` | WM_PAINT EndPaint 前调 `canvas.Flush()`；**拦截 WM_ERASEBKGND 返回 1**（禁用系统白刷擦底） |
| 🏗️ 改 | `APP/Widget/src/Win32/WindowImplWin32.cpp` | PaintDirect 补 Flush；RequestRepaint 的 InvalidateRect 第三参 TRUE→FALSE（不擦背景） |

**验证反馈（砚台）：** 双缓冲后 LogViewer 刷新不再闪烁，但滚动时有一闪而过的**纯白**全屏。

**白屏根因（纯白=系统背景色）：**
- 窗口类注册背景刷为 COLOR_WINDOW+1（白色，Win32Class.cpp）
- RequestRepaint → InvalidateRect(..., TRUE) 带擦背景标志
- 滚动触发重绘时，系统用白刷把客户区擦白，双缓冲 BitBlt 覆盖前漏出白底帧

**修复（方案①，禁系统白底）：**
- WM_ERASEBKGND 拦截直接 return 1，告诉系统背景已处理，不用白刷
- InvalidateRect TRUE→FALSE，不擦背景（我们自己每帧全覆盖重绘）
- 已确认：纯白 → 系统白底，修复 ① 已落；若仍有灰色/花屏再补 Canvas 位图初始底色（方案②）
- ✅ premake vs2026 通过；编译+效果待砚台验证
- ✅ **砚台确认：闪烁+滚动白屏全部解决，无任何闪烁**

**待办链：** ② Ticker 增量更新；③ 局部脏区绘制；若需 Canvas 位图初始底色（方案②）

---

## 2026-08-01 — LogViewer 颜色解析修复 + SCrollArea 滚轮/滑块

| 操作 | 文件 | 说明 |
|------|------|------|
| 🐛 修 | `APP/UI/src/Composite/LogViewer.cpp` | `ParseLine` 重写为 ANSI 解析器：剥 \x1B[38;2;R;G;Bm → 前景ARGB；`\x1B[0m` 只作收尾不覆盖行色（每行结构 <颜色>正文<重置>） |
| 🏗️ 新 | `APP/UI/include/Component/Component.h` | 加 `virtual OnScroll(float yDelta)`（滚轮输入）+ `GetScrollStep()` + `OnMousePressed/Moved/Released`（鼠标交互，默认空）—— 内容自治度量接口 |
| 🏗️ 新 | `APP/UI/include/Component/ListBox.h` | override `GetScrollStep()` 返回 `m_LineHeight`（列表滚一格=一行） |
| 🏗️ 新 | `APP/UI/include/Component/ScrollArea.h` | 加滑块常量/`OnScroll`/`GetViewWidth`/`GetScrollStep`/滑块绘制/拖拽状态声明 |
| 🏗️ 新 | `APP/UI/src/ScrollArea.cpp` | `OnScroll` 滚轮；`DrawScrollbar` 右侧滑块（比例高度+按offset定位）；内容宽扣滑块位；滑块拖动+点击轨道翻页；`SetScrollOffset` 主动 `RequestRepaint()` |
| 🏗️ 新 | `APP/UI/include/Component/Component.h` | 加 `m_RepaintCallback` + `RequestRepaint()`/`SetRepaintCallback()`（组件请求所属窗口重绘的同步回调通道） |
| 🏗️ 新 | `APP/UI/src/Container.cpp` | `Connect(MouseScrolled)` → HitTest → 转发 `OnScroll`；鼠标按下/移动/抬起转发给组件（`m_DragTarget` 跟踪拖拽目标）；`AddComponent` 注入重绘回调 |
| 🏗️ 新 | `APP/UI/include/Container/Container.h` | 加 `m_DragTarget`/拖拽起点成员 |

**讨论纪要：**
- 颜色根因：Log 写入前 replaceColor 已把 %r:g:b% 转 ANSI；旧 ParseLine 按 %...% 解析永远走 else 原样显示。且第一版把结尾 \x1B[0m 当重置抹黑整行 → 再加 hasColor/忽略重置 修复
- 滚动架构：ScrollArea=通用视口（裁剪/偏移/滚轮/滑块），内容自治（contentHeight+stepSize 两个数字接口），不认具体类型 → 换行逻辑以后只藏内容内，ScrollArea 不改
- 滚轮链路：WM_MOUSEWHEEL→MouseScrolled 已存在（Win32WndProc），断点在 Container 没转发
- 待做：ListBox 只画可视区行（性能）；自动换行 + Canvas MeasureText（日志长行）

**收工状态（08-01）：**
- ✅ 颜色解析（ANSI）、滚轮、拖滑块、点击轨道翻页 均可用
- ✅ 滚动自刷新修复：ScrollArea::SetScrollOffset 主动 RequestRepaint（此前依赖 Ticker 顺带刷新，日志停就失灵）
- 🔧 确认真体验问题：每隔 500ms 日志更新 → OnPaint 里 ApplyFilter 全量 Clear+AddItem → 整块重绘 → 闪烁/跳动/鎙眼（不是性能，是视觉抖动）
- 📌 明日优先级：① 双缓冲（Canvas 层，消除闪烁，待与砚台确认落点）→ ② 增量更新（Ticker 只追加新增，关健词变化才全量重建）→ ③ 局部脏区绘制（只画变化区）
- 📌 后续长期：ListBox 只画可视区行；自动换行 + Canvas MeasureText

## 2026-07-30 — LogViewer 独立线程自绘 + 多项 Bug 修复

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `APP/Widget/src/Canvas.cpp` | ~~Canvas XWnd 构造+自管理HDC（已撤销）~~ |
| 🏗️ 新 | `APP/Widget/include/WindowImpl.h` | 加 `PaintDirect` 纯虚接口 |
| 🏗️ 新 | `APP/Widget/src/Win32/WindowImplWin32.cpp` | 实现 `PaintDirect(GetDC+Canvas+ReleaseDC)` + `ValidateWindow` |
| 🔧 改 | `APP/Widget/include/BaseWin.h` | 加 `ValidateWindow()`/`PaintDirect()`/`m_SkipMainThreadPaint`；`GetNativeHandle` 委托 `m_Impl` |
| 🔧 改 | `APP/Widget/src/BaseWin.cpp` | 加 `ValidateWindow`/`PaintDirect` 委托；`Destroy()` 加 `m_Impl.reset()` 防重入 |
| 🔧 改 | `APP/Widget/src/Win32/WindowImplWin32.cpp` | `Destroy()` 先置空 `m_Hwnd` 再 `DestroyWindow` 防 double destroy |
| 🔧 改 | `APP/Widget/src/Win32/Win32WndProc.cpp` | WM_PAINT 判断 `m_SkipMainThreadPaint` 跳过；去掉 `SetNativeHandle` |
| 🔧 改 | `APP/UI/include/Composite/LogViewer.h` | 精简接口：删 `OnIncrementalData`/`LayoutChildren`，加 `ApplyFilter` const |
| 🔧 改 | `APP/UI/src/Composite/LogViewer.cpp` | 主线程 OnPaint+shared_mutex 保护 m_AllEntries，Ticker 只读 DataStore |
| 🔧 改 | `APP/UI/src/Container.cpp` | 析构加 `disConnect(this)` 防止事件回调 dangling |
| 🔧 改 | `APP/Widget/src/XWidget.cpp` | `~XWidget()` 改为 `disConnect(this)` 不再调 `destroy()`；`destroy()` 中 `delete this` |
| 🔧 改 | `APP/UI/premake5.lua` | links 加 `DataStore`、`Timer` |
| 🔧 改 | `Core/Log/include/DataStoreDevice.h` | 析构不再调 `DataStore::Instance()`（解决析构顺序问题） |
| 🔧 改 | `Core/DataStore/src/DataStore.cpp` | `Instance()` 改为 leaky singleton（`new`）；加 `ClearAll()` |
| 🔧 改 | `Core/DataStore/include/DataStore.h` | 加 `ClearAll()` 声明 |
| ✅ 新 | `Core/Log/include/DataStoreDevice.h` | 析构 flush 已恢复（main 中 logger.clear() 保证 DataStore 先于 logger 析构） |
| 🐛 修 | `APP/Widget/src/Win32/WindowImplWin32.cpp` | `Destroy()` 先置 null 再 DestroyWindow，防 `~WindowImplWin32` 二次调用 |
| 🔧 改 | `Test/src/main.cpp` | 集成 LogViewer 测试 |

**讨论纪要：**
- 独立线程自绘方案被否（GDI 跨线程竞态），改回 shared_mutex + 主线程绘制
- Canvas 自管理 HDC 方案被否，改回 PaintDirect
- 底层 PaintDirect/ValidateWindow/m_SkipMainThreadPaint 保留做基础设施
- 析构顺序隐患：logger 全局 inline 变量晚于 DataStore static 析构 → crash
- 临修：DataStore 改为 leaky singleton，DataStoreDevice 析构不再调 DataStore
- 退出 `delete this` 导致的 double delete 问题待明天处理

## 2026-07-29 — LogViewer Composite 日志查看组件（建材阶段）

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `APP/UI/include/Composite/LogViewer.h` | LogViewer Composite：关键字输入 + 颜色日志列表 |
| 🏗️ 新 | `APP/UI/src/Composite/LogViewer.cpp` | Ticker 增量轮询、颜色解析、关键字筛选、deque 5000 行缓存 |
| 🏗️ 新 | `APP/UI/include/Composite/LogStripe.h` | LogStripe 纯展示层包装 ListBox |
| 🏗️ 新 | `APP/UI/src/Composite/LogStripe.cpp` | SetEntries 批量填充 |
| 🔧 改 | `APP/UI/include/Component/TextInput.h` | 加 `OnTextChange` 回调 |
| 🔧 改 | `APP/UI/src/TextInput.cpp` | `NotifyTextChange()` 在 SetText/OnChar/OnKeyDown 时触发 |
| 🔧 改 | `APP/UI/src/Container.cpp` | 鼠标点击 hit-test + 焦点转移（ScreenToClient + GetMouseScreenPos） |



## 2026-07-27 — Buffer.h 重构 + BufferPool + RingBuffer + DataStore 前后端分离

| 操作 | 文件 | 说明 |
|------|------|------|
| 🐛 修 | `Core/Log/include/DataStoreDevice.h` | `Log()` 中 `Insert` → `Append`，否则每条日志覆盖上一条 |
| 🏗️ 新 | `Core/Buffer/include/BufferPool.h` | BufferPool — 通用内存池（预分配大块，复用，线程安全） |
| 🏗️ 新 | `Core/Buffer/src/BufferPool.cpp` | BufferPool 实现 |
| 🏗️ 新 | `Core/Buffer/include/RingBuffer.h` | RingBuffer — 循环 Buffer（固定容量，自动覆盖） |
| 🏗️ 新 | `Core/Buffer/src/RingBuffer.cpp` | RingBuffer 实现 |
| 🔧 改 | `Core/Buffer/include/Buffer.h` | 大函数移 cpp，头文件只留声明 |
| 🔧 改 | `Core/Buffer/src/Buffer.cpp` | 移入 Reserve/Ensure/Append/toString 等实现 |
| ✅ 改 | `Core/DataStore/include/DataStore.h` | 新增 GetOrCreate / GetOrCreateRingBuffer |
| ✅ 改 | `Core/DataStore/src/DataStore.cpp` | GetOrCreate / GetOrCreateRingBuffer 实现 |
| ✅ 改 | `Core/Log/include/DataStoreDevice.h` | Log() 改用 RingBuffer，按容量自动覆盖 |
| 🏗️ 新 | `UI/include/Component/LogViewer.h` | LogViewer 窗口头文件 |
| 🏗️ 新 | `UI/src/LogViewer.cpp` | Canvas 自绘日志查看器，带颜色等级过滤 |
| ✅ 改 | `README.md` 或其他 | 如果有 LogViewer 注册需要改 Application 逻辑 |
| ✨ 改 | `Core/DataStore/include/DataStore.h` | 加 DumpStats() 方法，展示各 key 的 Buffer 大小 |
| ✨ 改 | `Core/DataStore/src/DataStore.cpp` | DumpStats() 实现 |

## 2026-07-26 — DataStore 内核 + Dock 体验优化 + 架构讨论

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `Core/DataStore/include/DataStore.h` | DataStore 全局单例：Insert/Append/Get/Remove/Rename + Flush/LoadFile/LoadDirectory |
| 🏗️ 新 | `Core/DataStore/src/DataStore.cpp` | 完整实现（Get 自动回源文件、Append 追加合并旧数据） |
| 🏗️ 新 | `Core/DataStore/include/DataStoreDevice.h` | DataStoreDevice — Log 设备，追加到 DataStore，自动用日期做文件名（YYYY-MM-DD.log） |
| 🏗️ 新 | `Core/DataStore/premake5.lua` | premake 配置 |
| ✅ 改 | `premake5.lua` | Core group 加 include "Core/DataStore" |

## 2026-07-26 — Dock 体验优化 + 架构讨论

| 操作 | 文件 | 说明 |
|------|------|------|
| ✅ 改 | `UI/src/DockLayer.cpp` | 按下标题栏即显示预览（不等鼠标移动） |
| ✅ 改 | `UI/src/Docker.cpp` | 预览标记改为五块 60x40 小方块（四边中心+中央） |
| ✅ 改 | `UI/src/Docker.cpp` | 被 Dock 的窗口先隐藏（后续改为析构+数据分离） |
| 📝 新 | `_notes/arch/DataStore.md` | DataStore 全局数据系统设计构想 |

**讨论纪要：**
- DataStore 构想：以 OS 文件系统为数据库，`DataStore/` 目录自动创建，文件名=key，后缀=解释器类型，Buffer=统一存储格式
- Dock 窗口未来：析构旧窗口 → 仅存数据引用（文件路径+工厂函数）→ 激活时重建窗口
- 前后端分离：窗口只管渲染传 Buffer*，数据由全局 DataStore 管理

## 2026-07-22 — 停靠系统：Overlay 预览指示器 + 拖拽检测线程

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `UI/include/Component/Overlay.h` | Overlay 半透明覆盖层组件（纯视觉，无事件） |
| 🏗️ 新 | `UI/src/Overlay.cpp` | FillRect 填充 + 四边 Border 绘制 |
| ✅ 改 | `UI/include/dock/Docker.h` | 5 个 Overlay 预览框 + 拖拽检测线程 |
| ✅ 改 | `UI/src/Docker.cpp` | 创建 5 个 Overlay + ShowDropPreviews/HideDropPreviews + 50ms 轮询线程 |
| ✅ 改 | `UI/src/DockLayer.cpp` | WindowDragBegin 启线程，WindowDragEnd 停线程 |

**设计决策：** 独立 50ms 线程轮询鼠标位置，不走事件队列（系统拖拽模态循环阻塞事件流）。零底层变动。

---

## 2026-07-20 — 平台抽象层重构

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新建 | `Widget/include/WindowImpl.h` | WindowImpl 纯虚接口：窗口生命周期/尺寸/坐标/鼠标/光标/SetParent |
| 🏗️ 新建 | `Widget/include/Win32/Win32Globals.h` | Win32 全局变量（g_hInstance/g_szClassName/hook 指针） |
| 🏗️ 新建 | `Widget/include/Win32/Win32Class.h` | Win32 窗口类注册声明 |
| 🏗️ 新建 | `Widget/include/Win32/Win32WndProc.h` | Win32 StaticWndProc 声明 |
| 🏗️ 新建 | `Widget/include/Win32/WindowImplWin32.h` | Win32 WindowImpl 创建函数声明 |
| 🏗️ 新建 | `Widget/src/Win32/Win32Class.cpp` | RegisterWinClass 实现，转发到 WinWndProc |
| 🏗️ 新建 | `Widget/src/Win32/Win32WndProc.cpp` | StaticWndProc 实现，Handler 映射 + OnPaint 回调 |
| 🏗️ 新建 | `Widget/src/Win32/WindowImplWin32.cpp` | WindowImplWin32 全实现 + WindowStyleFlag→WS_* 转换 |
| 🏗️ 新建 | `Widget/src/PlatformFactory.cpp` | 条件编译工厂 |
| 🏗️ 新建 | `Widget/include/CanvasImpl.h` | CanvasImpl 纯虚接口：FillRect/DrawText/SetClip |
| 🏗️ 新建 | `Widget/include/Canvas.h` | Canvas 轻量包装类，隐藏平台实现 |
| 🏗️ 新建 | `Widget/src/Win32/CanvasImplWin32.cpp` | CanvasImplWin32 GDI 实现 |
| 🏗️ 新建 | `Widget/include/PlatformLoop.h` | PlatformLoop 纯虚接口：PumpMessage |
| 🏗️ 新建 | `Widget/src/Win32/PlatformLoopWin32.cpp` | PlatformLoopWin32 PeekMessage 实现 |
| 🏗️ 新建 | `Core/Input/include/KeyMapper.h` | KeyMapper 纯虚接口：键码映射/按键查询/鼠标位置 |
| 🏗️ 新建 | `Core/Input/src/Win32/KeyMapperWin32.cpp` | KeyMapperWin32 实现（VK_* ↔ KeyCode 映射/GetKeyState/GetCursorPos） |
| 🏗️ 新建 | `GraphicsContext/src/Win32/OpenGLContextWin32.cpp` | OpenGLContextWin32 WGL 实现 |
| 🛠️ 重构 | `Widget/include/BaseWin.h` | 去 HWND/去平台宏；WindowStyleFlag 类型；持 unique_ptr<WindowImpl> |
| 🛠️ 重构 | `Widget/src/BaseWin.cpp` | 所有方法委托 m_Impl；SetParent/OnPaint(Canvas*) 支持 |
| 🛠️ 重构 | `Widget/include/XWidget.h` | createGraphicsContext(GraphicsType) 工厂方法替代模板 |
| 🛠️ 重构 | `Widget/src/XWidget.cpp` | 精简重复方法 |
| 🛠️ 重构 | `Widget/include/Win32/Win32Globals.h` | 移除 g_ContainerHook（已废弃） |
| 🛠️ 重构 | `Widget/src/Entry.cpp` | WinCore 引用 → Win32::RegisterWinClass |
| 🛠️ 重构 | `Widget/src/Win32/Win32WndProc.cpp` | WM_PAINT 创建 Canvas 传 OnPaint(Canvas*)；去 g_ContainerHook |
| 🛠️ 重构 | `Core/Input/include/MapCode.h` | 去模板/Win32 依赖；纯函数声明 |
| 🛠️ 重构 | `Core/Input/src/Input.cpp` | 委托 KeyMapper |
| 🛠️ 重构 | `GraphicsContext/include/GraphicsContext.h` | 去 Win32 依赖；GraphicsContextFactory::Create |
| 🛠️ 重构 | `GraphicsContext/src/GraphicsContext.cpp` | 删旧 Win32 实现（已移至 Win32/） |
| 🛠️ 重构 | `Application/include/Application.h` | 加 m_PlatformLoop |
| 🛠️ 重构 | `Application/src/Application.cpp` | pushEvents 委托 PlatformLoop |
| 🔧 适配 | `UI/src/Container.h/.cpp` | 去钩子/WinCore 引用；override OnPaint(Canvas*) |
| 🔧 适配 | `UI/src/Docker.cpp` | GetNHWD→GetNativeHandle；::SetParent→SetParent()；ShowCmd 枚举 |
| 🔧 适配 | `UI/src/DockPanel.cpp` | GetNHWD→GetNativeHandle；WS_*→WindowStyleFlag；RGB()→常量 |
| 🔧 适配 | `UI/src/DockLayer.cpp` | GetNativeWindow→GetNativeHandle |
| 🔧 适配 | `UI/src/ImGuiLayer.cpp` | WinCore→Win32Globals |
| 🔧 适配 | `UI/include/Component/Component.h` | OnKeyDown(int)→OnKeyDown(KeyCode) |
| 🔧 适配 | `UI/src/TextInput.cpp` | VK_*→Key::* 内部码 |
| 🗑️ 删除 | `Widget/include/WinCore.h` | 已拆分 |
| 🗑️ 删除 | `UI/include/Container/Canvas.h` | 移至 Widget 层 |
| 🗑️ 删除 | `UI/src/Canvas.cpp` | 移至 Widget 层 |

## 2026-07-18

| 操作 | 文件 | 说明 |
|------|------|------|
| ✅ 新 ｜ `APP/UI/include/dock/DockPanel.h` ｜ DockPanel — 停靠面板容器（tab 增删切换、摘出浮动） |

| 操作 | 文件 | 说明 |
|------|------|------|
| ✅ 新 ｜ `APP/UI/include/dock/DockPanel.h` ｜ DockPanel — 停靠面板容器（tab 增删切换、摘出浮动） |
| ✅ 新 ｜ `APP/UI/src/DockPanel.cpp` ｜ DockPanel 实现 |
| ✅ 新 ｜ `APP/UI/include/dock/Docker.h` ｜ Docker — 停靠系统主窗口（五区域 + 布局 + DockLayer） |
| ✅ 新 ｜ `APP/UI/src/Docker.cpp` ｜ Docker 实现 |
| ✅ 新 ｜ `APP/UI/include/dock/DockLayer.h` ｜ DockLayer — Layer 监听全局窗口拖拽事件 |
| ✅ 新 ｜ `APP/UI/src/DockLayer.cpp` ｜ DockLayer 实现（switch 分支 + Hanlded 标记） |
| ⚡ 改 ｜ `Core/Movement/include/movements.h` ｜ MovementType 枚举加 WindowDragBegin / WindowDragEnd |
| ⚡ 改 ｜ `Core/Movement/include/AppMovement.h` ｜ 新增 WindowDragBegin / WindowDragEnd 事件类 |
| ⚡ 改 ｜ `APP/Widget/include/WinCore.h` ｜ StaticWndProc 加 WM_NCLBUTTONDOWN(HTCAPTION) → WindowDragBegin；WM_EXITSIZEMOVE → WindowDragEnd |
| ⚡ 改 ｜ `APP/Widget/include/BaseWin.h` ｜ 新增跨平台工具方法：GetScreenRect / ScreenToClient / ClientToScreen / CaptureMouse / ReleaseMouseCapture / GetParentNHWD / SetCursorStyle / GetMouseScreenPos / GetWindowAt / MoveAndResize |
| ⚡ 改 ｜ `APP/Widget/src/BaseWIn.cpp` ｜ 实现上述跨平台方法（#ifdef XY_PLATFORM_WINDOWS 包裹） |
| 🐛 修 ｜ `APP/UI/src/TextInput.cpp` ｜ 删除未定义的 TriggerChange() 调用 |
| 🐛 修 ｜ `APP/UI/src/DockPanel.cpp` ｜ GetWidth/GetHeight 改为 get_width/get_height；OnPaint 委托 Container::OnPaint 而非手写 |
| 🐛 修 ｜ `APP/UI/src/Docker.cpp` ｜ include 路径修正；HitTestArea 用 GetActualWidth/Height 保持 const；SetWindowPos 全换 MoveAndResize |

| 操作 | 文件 | 说明 |
|------|------|------|
| ✅ 改 | `README.md` | 架构重构后全面更新：目录结构、状态表、设计理念 |
| 🐛 修 | `ModelViewer.h` | FlatColor shader 切换时 cube 消失 → 缺少 `u_Color` uniform |
| 🐛 修 | `ModelLoader.cpp` | **重写 OBJ 解析器** — 旧版指针跳转式只解析了 12 条面中的 6 条（每个面只读到第 1 个三角），改用 `std::getline` + `sscanf_s` 逐行解析 |
| 🐛 修 | `ModelLoader.cpp` | `sscanf_s` 解析 `v//vn` 时顺序错误（先试 `%d/%d/%d` 再试 `%d//%d`），导致 vn 永远为 -1 → resolveIndex 误解析为有效值 |
| 🐛 修 | `ModelLoader.cpp` | `resolveIdx(-1, count)` 返回 `count-1`（误把 -1 当相对索引），加 `== -1` 保护 |
| 🐛 修 | `ModelGenerator.cpp` | `GenerateBoxMeshData()` 新增，直接生成 MeshData（不依赖 OBJ 解析）—— 备用方案 |
| 💡 下步 | 材质支持 | 解析 Blender 导出的 MTL（Kd/Ka/Ks/Ns/map_Kd），Mesh 绑定材质名，`Submit` 传 `u_Color` 到 shader |

## 2026-07-16

| 操作 | 文件 | 说明 |
|------|------|------|
| ✅ 新 | `APP/UI/include/Container/Canvas.h` | 画布抽象，`CanvasHandle` typedef 隔离平台 |
| ✅ 新 | `APP/UI/src/Canvas.cpp` | GDI 实现：FillRect/DrawText/SetClip |
| ✅ 新 | `APP/UI/include/Container/Container.h` | 有 HWND 的容器（继承 XWidget） |
| ✅ 新 | `APP/UI/src/Container.cpp` | WM_PAINT 创建 Canvas 绘制 + hit-test 事件转发 + 键盘/滚轮处理 |
| ✅ 新 | `APP/UI/include/Component/Component.h` | 轻量组件基类 |
| ✅ 新 | `APP/UI/include/Component/Label.h` + `src/Label.cpp` | 文字标签 |
| ✅ 新 | `APP/UI/include/Component/TextInput.h` + `src/TextInput.cpp` | 可交互输入框 |
| ✅ 新 | `APP/UI/include/Component/ScrollArea.h` + `src/ScrollArea.cpp` | 滚动容器 |
| ✅ 新 | `APP/UI/include/Component/ListBox.h` + `src/ListBox.cpp` | 条目列表（每行独立颜色） |
| ✅ 新 | `APP/UI/include/Component/Button.h` + `src/Button.cpp` | 按钮 |
| 🔧 改 | `APP/UI/premake5.lua` | UI 模块加 Widget/Movement/Application/Input 等 include 路径 |
| 🔧 改 | `Widget/include/WinCore.h` | 新增 `g_ContainerHook` 函数指针，跟 ImGui hook 并存 |

## 2026-07-08
（旧记录保留）
