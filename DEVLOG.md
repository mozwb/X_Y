## 2026-07-24 — Timer 模块新增 Ticker + Docker 改用 Ticker

| 操作 | 文件 | 说明 |
|------|------|------|
| 🏗️ 新 | `Core/Timer/include/Timer.h` | Ticker 类声明：Start/Stop/Join/Pause/Resume/Restart/Detach |
| 🛠️ 改 | `Core/Timer/src/Timer.cpp` | Ticker 全实现：condition_variable + wait_for + notify_one 即时唤醒 |
| 🛠️ 改 | `APP/UI/include/dock/Docker.h` | 去 std::thread/atomic 成员，改持 Ticker m_DragTicker |
| 🛠️ 改 | `APP/UI/src/Docker.cpp` | 去裸线程管理，StartDragMonitor 调 Ticker::Start，StopDragMonitor 调 Pause |
| 🛠️ 改 | `APP/UI/premake5.lua` | 补 Timer 的 include 和 link 依赖 |

**设计决策：**
- Ticker 作为 Timer 模块的通用定时器，独立线程 + condition_variable 实现即时唤醒
- wait_for 替代 sleep_for，Stop() 通过 notify_one 即时叫醒线程，不等周期结束
- Pause/Resume 控制回调执行，不销毁线程
- 析构自动 Stop+Join，安全兜底
- Docker 不再自己管理线程，全部委托 Ticker
