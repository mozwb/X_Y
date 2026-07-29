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

## 2026-07-24 — 自定义窗口拖拽 + Ticker + Docker 停靠重构

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `Core/Timer/src/Timer.cpp` | Ticker 类实现：condition_variable + wait_for + notify_one 即时唤醒 |
| 🏗️ 新 | `Core/Timer/include/Timer.h` | Ticker 声明：Start/Stop/Join/Pause/Resume/Restart/Detach |
| 🛠️ 改 | `APP/Widget/include/BaseWIn.h` | 加 m_IsDragging/m_DragOffsetX/m_DragOffsetY 拖拽状态 |
| 🛠️ 改 | `APP/Widget/src/Win32/Win32WndProc.cpp` | WM_NCLBUTTONDOWN 只拦截 HTCAPTION（自定义拖拽 + SetCapture），其他非客户区（关闭/最小化/resize）break 走 DefWindowProc；WM_LBUTTONUP 只拖拽时 return 0；修复非客户区按钮失效 bug |
| 🛠️ 改 | `APP/UI/include/dock/Docker.h` | 去掉 Ticker/StartStopDragMonitor/PollMousePosition |
| 🛠️ 改 | `APP/UI/src/Docker.cpp` | 完全去掉线程相关代码 |
| 🛠️ 改 | `APP/UI/src/DockLayer.cpp` | MouseMoved 事件中实时更新预览（事件驱动替代线程轮询）|
| 🛠️ 改 | `APP/UI/premake5.lua` | 去掉 Timer 依赖 |

**设计决策：**
- **自定义拖拽**：系统拖拽模态循环阻塞消息泵，改为 SetCapture + 自己处理 WM_MOUSEMOVE 移动窗口
- 拖拽过程中消息泵正常运转，MouseMoved 事件照常派发，DockLayer 能实时收到并更新预览
- Ticker 保留作为通用定时器（Timer 模块），Docker 不再使用
- DockLayer 从轮询改为事件驱动：MouseMoved 到达时直接更新指示器
