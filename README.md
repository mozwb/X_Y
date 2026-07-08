# X_Y

我的代码库。一个 Windows 独占的 C++20 图形引擎 / 工具包，从零搭建（基于 Hazel 架构思路）。

---

## 项目结构

```
X_Y/
├── Core/                  # 核心基础设施
│   ├── Application/       #   应用生命周期、事件循环
│   ├── Buffer/            #   内存缓冲区工具
│   ├── FilesSystem/       #   文件读写 + Win32 对话框
│   ├── GraphicsContext/   #   OpenGL 上下文封装
│   ├── Image/             #   图片解析（自研，封装 pngdec）
│   ├── Input/             #   键盘/鼠标按键映射
│   ├── Log/               # 🟢 自研彩色日志系统（最成熟）
│   ├── Math/              #   数学库（glm 封装，Policy 模式）
│   ├── Model/             #   OBJ 模型解析 + GPU 数据转换
│   ├── Movement/          #   事件系统（信号槽 + 队列）
│   ├── Timer/             #   计时 / 性能分析
│   ├── UI/                #   ImGui 集成层
│   ├── Window/            #   窗口系统（Win32 封装）
│   └── XCore/             #   通用工具（字符串转换等）
├── Render/                # OpenGL 渲染抽象层
│   ├── include/           #   VertexArray / Shader / Buffer / Texture / Camera
│   └── src/               #   具体实现
├── Test/                  # 测试入口 / 模型查看器
├── vendor/                # 第三方库（glad, glm, premake）
├── assets/                # 测试用模型、纹理
├── premake5.lua           # 构建配置
└── xypch/                 # 预编译头
```

---

## 当前状态

| 模块 | 状态 | 备注 |
|------|------|------|
| Log | ✅ 完工 | 带颜色格式化日志，多输出设备 |
| Window | ✅ 可用 | Win32 封装，父子窗口，WndProc hook |
| Movement | ✅ 可用 | 信号槽事件系统 |
| Buffer | ✅ 完工 | Rule of 5，自动扩容 |
| Timer | ✅ 可用 | StopWatch + RAII ProfileScope |
| Math | 🔧 基础 | glm 封装，Policy 模式，可替换 |
| Input | 🔧 基础 | 按键码映射表 |
| GraphicsContext | ✅ 可用 | OpenGL 上下文创建 |
| Render | 🟢 核心完备 | VAO/VBO/IBO/Shader/Texture/Camera/DrawCall |
| UI (ImGui) | ✅ 可用 | 完全解耦，WndProc hook 接入 |
| FileSystem | ✅ 可用 | 文件读写 + Win32 对话框 |
| Image | ✅ 可用 | PNG 解码 + Texture2D 自动加载 |
| Model | 🟢 解析完成 | OBJ v/vt/vn/f/o/g/usemtl，GPU 转换可用 |
| Model Viewer | 🚧 开发中 | ImGui 界面 + EditorCamera 待集成 |

---

## 构建

- 需要 **Visual Studio 2026** + C++20
- 运行 `PGenerateProject.bat` 生成 `.slnx`
- 在 VS 中编译并运行 `Test` 项目

---

## 设计理念

- **模块化**：每个 Core 子项目独立 lib，按需 link
- **解耦**：Window 不知道 ImGui 存在，Render 不知道 Model 存在
- **自研优先**：日志、图片解码、模型解析都自己写，不依赖庞大第三方库
- **渐进增强**：先跑通再优化，不做超前设计

---

_由 💎 琉璃(ai)维护_
