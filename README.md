# X_Y

我的代码库。一个 Windows 独占的 C++20 图形引擎 / 工具包，从零搭建（架构思路受 Hazel 启发，架构已重构）。

---

## 项目结构

```
X_Y/
├── Core/                  # 🔩 核心基础设施（独立 lib）
│   ├── Buffer/            #   内存缓冲（已并入 Memory，见下）
│   ├── DataStore/         # 🅱️ 全局数据系统（以 OS 文件系统为库，Buffer=存储格式）
│   ├── FilesSystem/       #   文件读写 + Win32 对话框
│   ├── Image/             #   图片解析（自研，封装 pngdec）
│   ├── Input/             #   键盘/鼠标按键映射
│   ├── Log/               # 🟢 自研彩色日志系统（最成熟）+ DataStoreDevice 后端
│   ├── Memory/            #   Buffer 内存池 + RingBuffer + LoopQueue 环形队列
│   ├── Middle/            #   中间数据结构（MeshData）
│   ├── Timer/             #   计时 / 性能分析（含 Ticker 周期线程定时器）
│   ├── XCore/             #   通用工具（字符串转换、类型萃取等）
│   └── XMath/             #   数学库（Policy 模式，可替换实现）
│
├── APP/                   # 🏢 上层应用基础设施（独立 lib）
│   ├── Application/       #   应用生命周期、消息循环
│   ├── GraphicsContext/   #   OpenGL 上下文封装
│   ├── Movement/          #   事件系统（信号槽 + 队列 + 分发器）
│   ├── UI/                #   自研原生 UI（Container/Component 两层）+ 原生组件
│   └── Widget/            #   窗口系统（Win32 封装 + XWidget）+ Canvas/Font/DPI 平台抽象
│
├── Render/                # 🎨 OpenGL 渲染抽象层
│   ├── Model/             #   OBJ 模型加载 + 立方体生成 + GPU 转换
│   └── Render/            #   VAO/VBO/IBO/Shader/Texture/Camera/
│                           #   FrameBuffer/UniformBuffer + OpenGL 实现
│
├── Test/                  # 🧪 测试入口 / 模型查看器 / 日志查看器
├── MD/                    # 📖 模块文档（Log、Window 等）
├── vendor/                #   第三方库（glad, glm, imgui, premake）
├── assets/                #   测试用模型、纹理、Shader
├── premake5.lua           #   构建配置
└── xypch/                 #   预编译头
```

---

## 当前状态

| 模块 | 状态 | 备注 |
|------|------|------|
| Log | ✅ 完工 | 带颜色格式化日志，多输出设备，模板化 Level 可拓展 |
| XWidget | ✅ 可用 | Win32 封装，父子窗口，XWidget/RenderWin 两层抽象 |
| Movement | ✅ 可用 | 信号槽 + 枚举类型分发 + 层栈，支持拓展事件 |
| Application | ✅ 可用 | 应用生命周期，单例 + 消息队列 + 分发器 |
| Buffer | ✅ 完工 | Rule of 5，自动扩容 |
| Timer | ✅ 可用 | StopWatch + ProfileScope + Ticker（周期线程定时器）|
| XMath | ✅ 可用 | glm 封装，Policy 模式，可替换数学后端 |
| Input | ✅ 可用 | 按键码映射表（Win32 ↔ 内部码） |
| GraphicsContext | ✅ 可用 | OpenGL 上下文创建 |
| Render | 🟢 核心完备 | VAO/VBO/IBO/Shader/Texture/Camera/FrameBuffer/UniformBuffer + EditorCamera |
| UI (ImGui) | ✅ 可用 | ImGuiLayer 集成 + 自研原生组件（Container/Component 两层）|
| FileSystem | ✅ 可用 | 文件读写 + Win32 对话框 |
| Image | ✅ 可用 | PNG 解码 + 纹理自动加载 |
| Middle/MeshData | ✅ 可用 | 中间数据结构，桥接 Model ↔ Render |
| Model Loader | 🟢 解析完成 | OBJ v/vt/vn/f/o/g/usemtl，去重 → interleaved |
| Model Generator | ✅ 可用 | 立方体 .obj 生成（每面独立法线） |
| Model Viewer | 🚧 开发中 | ImGui Shader 切换 + RenderWin 渲染 |
| UI 原生组件 | ✅ 可用 | Container/Component 两层：Label/ListBox/TextInput/Button/ScrollArea/Overlay |
| Memory | ✅ 可用 | Buffer 内存池 + RingBuffer 循环缓冲 + LoopQueue 环形队列模板 |
| DataStore | ✅ 可用 | 全局单例数据系统，以 OS 文件系统为库，自动建目录，文件=key，Buffer=存储 |
| Font 抽象 | ✅ 可用 | FontDesc/Font 封装（微软雅黑 ClearType 默认，支持自定义 ttf/otf 私有加载）+ 度量 |
| Canvas 双缓冲 | ✅ 可用 | 内存帧一次上屏 + 拦截 WM_ERASEBKGND，消除刷新/滚动闪烁 |
| Canvas 组合拳 | ✅ 可用 | `FillText` 铺背景+同背景色写字（ClearType 亚像素用真实背景），函数尾恢复透明 |
| LogViewer Composite | 🚧 开发中 | 关键字筛选(增量) + 颜色日志列表 + LoopQueue 5000 行 + 可视裁剪 + 双缓冲 + Font 抗锯齿 |
| Dock 停靠系统 | 🚧 开发中 | Docker 五区域布局 + DockPanel tab 管理 + 自定义拖拽预览 |
| 平台抽象层 | ✅ 可用 | 纯虚+工厂模式：WindowImpl/CanvasImpl/FontImpl/PlatformLoop/KeyMapper，UI 层无 HWND/HDC |
| 全局 UTF-8 | ✅ 已统一 | premake /utf-8 + 显示端 UTF-8→Wide，全链路跨语言编码 |
| DPI 适配 | ✅ 路线A | DPI-aware + 底层逻辑↔物理坐标空间映射，上层全逻辑零改动 |
| BaseWin 双坐标系 | ✅ 可用 | 逻辑坐标(默认) + Physical 后缀(真实像素)，供系统级功能(DockLayer) |

---

## 近期优化

- **架构重构**：Core 精简为纯粹基础设施；APP 独立为上层应用框架
- **Render 分层**：拆为 Model（数据导入）和 Render（管线渲染）两个子目录
- **Middle 层**：MeshData 作为模型 → GPU 的通用中间数据结构
- **RenderWin**：从 XWidget 分离出渲染专用窗口，不渲染的窗口不继承
- **EditorCamera**：轨道控制相机（鼠标滚轮缩放、右键旋转、中键平移）
- **Shader Library**：自动加载目录下所有 .glsl，按文件名索引
- **LoopQueue**：通用固定容量循环队列模板，LogViewer 底层 deque 替换（满则覆盖最旧，At(seq) 增量访问）
- **LogViewer 性能链**：增量筛选 → 可视裁剪(只画屏上行) → 双缓冲 → Font 抗锯齿
- **Font 抽象**：微软雅黑 ClearType 默认抗锯齿 + 支持自定义 ttf 私有加载
- **全局 UTF-8**：premake `/utf-8` + 显示端 UTF-8→Wide，跨语言（中文/外文）编码统一
- **DPI 路线A**：声明 DPI-aware + 底层逻辑↔物理坐标映射，彻底解决高 DPI 下字体模糊

---

## 编码 & DPI 约定（改代码前必看）

- **全链路 UTF-8**：源码是 UTF-8，工程全局 `/utf-8` 编译；字符串内部统一 UTF-8，只在显示/写文件边界转 Wide。
- **不清动存储层编码**：FilesSystem/DataStore 字节直通（std::ios::binary），`/utf-8` 后进出天然 UTF-8，不需改。
- **坐标：上层全逻辑，底层 Canvas 转换**：UI 层（布局/命中/停靠）一律用逻辑坐标；Canvas 内部 ×scale 成物理像素，鼠标输入 ÷scale 回逻辑。
- **BaseWin 双坐标系**：默认方法返回逻辑坐标；需要真实像素（跨窗口/系统 API）用 `Physical` 后缀版本（如 `ScreenToClientPhysical`）。
- **DPI 全自动**：`Dpi::GetScale()` 运行时读取系统 DPI，用户改缩放实时更新，无需配置。
- **写密文用宽字符**：Canvas 窄字符 UTF-8→Wide 再 TextOut；不直接用 TextOutA（那会走系统 ANSI 代码页，乱码）。

---

## 构建

- 需要 **Visual Studio 2026** + C++20
- 运行 `PGenerateProject.bat` 生成 `.slnx`
- 在 VS 中编译并运行 `Test` 项目

---

## 设计理念

- **模块化**：每个项目独立 lib，按需 link
- **分层解耦**：Core → 纯工具；APP → 应用框架；Render → 渲染管线
- **自研优先**：日志、图片解码、模型解析、UI 组件都自己写，不依赖庞大第三方库
- **自定义窗口拖拽**：不依赖系统拖拽模态循环，消息泵不阻塞
- **渐进增强**：先跑通再优化，不做超前设计

---

_由 💎 琉璃(ai)维护_
