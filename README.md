# X_Y

我的代码库。一个 Windows 独占的 C++20 图形引擎 / 工具包，从零搭建（架构思路受 Hazel 启发，架构已重构）。

---

## 项目结构

```
X_Y/
├── Core/                  # 🔩 核心基础设施（独立 lib）
│   ├── Buffer/            #   内存缓冲区工具
│   ├── FilesSystem/       #   文件读写 + Win32 对话框
│   ├── Image/             #   图片解析（自研，封装 pngdec）
│   ├── Input/             #   键盘/鼠标按键映射
│   ├── Log/               # 🟢 自研彩色日志系统（最成熟）
│   ├── Middle/            #   中间数据结构（MeshData）
│   ├── Timer/             #   计时 / 性能分析
│   ├── XCore/             #   通用工具（字符串转换、类型萃取等）
│   └── XMath/             #   数学库（Policy 模式，可替换实现）
│
├── APP/                   # 🏢 上层应用基础设施（独立 lib）
│   ├── Application/       #   应用生命周期、消息循环
│   ├── GraphicsContext/   #   OpenGL 上下文封装
│   ├── Movement/          #   事件系统（信号槽 + 队列 + 分发器）
│   ├── UI/                #   ImGui 集成层（ImGuiLayer）
│   └── Widget/            #   窗口系统（Win32 封装 + XWidget）
│
├── Render/                # 🎨 OpenGL 渲染抽象层
│   ├── Model/             #   OBJ 模型加载 + 立方体生成 + GPU 转换
│   └── Render/            #   VAO/VBO/IBO/Shader/Texture/Camera/
│                           #   FrameBuffer/UniformBuffer + OpenGL 实现
│
├── Test/                  # 🧪 测试入口 / 模型查看器
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
| Dock 停靠系统 | 🚧 开发中 | Docker 五区域布局 + DockPanel tab 管理 + 自定义拖拽预览 |

---

## 近期优化

- **架构重构**：Core 精简为纯粹基础设施；APP 独立为上层应用框架
- **Render 分层**：拆为 Model（数据导入）和 Render（管线渲染）两个子目录
- **Middle 层**：MeshData 作为模型 → GPU 的通用中间数据结构
- **RenderWin**：从 XWidget 分离出渲染专用窗口，不渲染的窗口不继承
- **EditorCamera**：轨道控制相机（鼠标滚轮缩放、右键旋转、中键平移）
- **Shader Library**：自动加载目录下所有 .glsl，按文件名索引

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
