// Shell：把一行文本变成一条条 run 指令，再交给内核执行。
// 内核只认 run 这一个动词；「直接把命令名写出来」是默认解析器补的 run，
// 以后要写别的语法（变量、表达式、条件）时，只换解析器，内核不动。
#pragma once

#include <string>
#include <vector>

namespace X_Y
{
    struct EngineContext;

    // 一条待执行的内核指令。verb 目前只会是 "run"。
    struct Instruction
    {
        std::string verb;
        std::vector<std::string> args; // args[0] = 命令名
    };

    // 默认解析器：按空白切词。
    //   "run alloc 1024"  → {run, [alloc, 1024]}
    //   "alloc 1024"      → {run, [alloc, 1024]}   （补上 run）
    //   ""                → {}                      （空行，不产生指令）
    std::vector<Instruction> ParseLine(const std::string& line);

    // 执行一条指令。目前只接受 verb == "run"。返回退出码。
    int Execute(EngineContext& ctx, const Instruction& ins);

    // 交互循环；返回最后一条命令的退出码。EOF 时正常退出。
    int RunShell(EngineContext& ctx, bool echoPrompt);
}
