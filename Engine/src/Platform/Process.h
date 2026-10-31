#pragma once
/*
Platform/Process.h —— 启动外部进程、等它结束、可选捕获输出。

只做第三类命令需要的那点事：
  - 起一个 exe（不走 shell，不解释 | > && 通配符）
  - 阻塞到它退出，拿退出码
  - 可选把 stdout + stderr 合并收进一个字符串

不做：
  - 超时 / 取消 / 流式回调
  - stdout / stderr 分开（v1 合并；要分开再上 poll/线程）
  - 环境变量注入 / 改 cwd
  - 交互式 stdin 转发（v1 继承当前 stdin）

退出码：
  正常退出        -> 子进程退出码
  POSIX 被信号杀  -> 128 + signal
  启动失败        -> Started=false, ExitCode=-1, Error 写原因

平台代码只在 Platform/Win32/ProcessWin32.cpp 与
Platform/Posix/ProcessPosix.cpp 里；这个头不出现 windows.h / unistd.h。
*/

#include <string>
#include <string_view>
#include <vector>

namespace X_Y::Process
{
    struct Options
    {
        // 合并捕获 stdout + stderr；不开就继承当前终端。
        bool Capture = false;
    };

    struct Result
    {
        bool Started = false;
        int ExitCode = -1;
        std::string Error;

        std::string Output; // Capture 时有效：stdout + stderr 合并

        bool Ok() const
        {
            return Started && ExitCode == 0 && Error.empty();
        }
    };

    // arguments 不含 program 本身；内部会把它作为 argv[0]。
    // program 可以是路径，也可以只是名字（走 PATH）。
    Result Run(std::string_view program,
               const std::vector<std::string>& arguments,
               Options options = {});
}
