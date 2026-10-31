#pragma once
/*
Shell.h —— 内核的交互前端，不是命令分发本身。

v1 只做薄壳：
  - stdin 逐行读（std::getline）
  - 分词支持单/双引号、反斜杠转义（路径带空格能过）
    Windows 路径在 shell 里请用引号或正斜杠：引号外的 \ 是转义符
  - 第一个词是 run 就丢掉；其余查 ctx.commands
  - 调命令前把 currentModule 设成命令 owner，退出恢复
  - exit / quit 只由 RunShell 处理

不做：
  - 行编辑 / 历史 / Tab 补全      -> 以后拆成独立 shell program 再说
  - 未知命令自动起外部进程         -> 和内核“只认 run”冲突
  - 管道 / 重定向 / 变量展开       -> 以后

退出码：
  0 成功，2 用法/解析错误，3 找不到命令，其余是命令自己的返回值。
*/

#include <string>
#include <vector>

namespace X_Y
{
    struct EngineContext;

    // 直接执行一个已经分好词的 argv；给 --run / --program 用，
    // 不用把参数再拼回一行字符串。
    int ExecuteArgs(EngineContext& ctx, const std::vector<std::string>& args);

    // 执行一行；不处理 exit/quit。空行返回 0。
    int ExecuteLine(EngineContext& ctx, const std::string& line);

    // 从 stdin 循环读，直到 EOF / exit / quit。
    // prompt=true 且 ctx.interactive 时打 "> " 提示符。
    // 返回最后一条命令的退出码。
    int RunShell(EngineContext& ctx, bool prompt);
}
