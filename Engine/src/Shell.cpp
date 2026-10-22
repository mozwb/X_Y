#include "Shell.h"

#include "CommandTable.h"
#include "Host.h"

#include <xy_command.h>

#include <cstdio>
#include <cstring>
#include <iostream>

namespace xy
{
    namespace
    {
        const char* kRunVerb = "run";

        std::vector<std::string> Tokenize(const std::string& line)
        {
            std::vector<std::string> out;
            size_t i = 0;
            while (i < line.size())
            {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
                {
                    ++i;
                }
                const size_t start = i;
                while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
                {
                    ++i;
                }
                if (i > start)
                {
                    out.push_back(line.substr(start, i - start));
                }
            }
            return out;
        }
    }

    std::vector<Instruction> ParseLine(const std::string& line)
    {
        const auto tokens = Tokenize(line);
        if (tokens.empty())
        {
            return {};
        }

        Instruction ins;
        ins.verb = kRunVerb;
        if (tokens[0] == kRunVerb)
        {
            // 已经显式写了 run：后面第一个词才是命令名
            if (tokens.size() > 1)
            {
                ins.args.assign(tokens.begin() + 1, tokens.end());
            }
        }
        else
        {
            ins.args = tokens;
        }
        return {ins};
    }

    int Execute(EngineContext& ctx, const Instruction& ins)
    {
        if (ins.verb != kRunVerb)
        {
            std::fprintf(stderr, "内核只认 run，收到 '%s'\n", ins.verb.c_str());
            return 2;
        }
        if (ins.args.empty())
        {
            std::fprintf(stderr, "run 后面缺少命令名\n");
            return 2;
        }

        CommandTable table;
        table.Refresh(ctx.services);

        const auto* cmd = table.Find(ins.args[0]);
        if (!cmd)
        {
            std::fprintf(stderr, "没有命令 '%s'（用 run help 看全部）\n", ins.args[0].c_str());
            return 3;
        }

        std::vector<const char*> argv;
        argv.reserve(ins.args.size() + 1);
        for (const auto& s : ins.args)
        {
            argv.push_back(s.c_str());
        }
        argv.push_back(nullptr);

        return cmd->invoke(cmd->self, static_cast<int>(ins.args.size()), argv.data());
    }

    int RunShell(EngineContext& ctx, bool echoPrompt)
    {
        int last = 0;
        std::string line;
        for (;;)
        {
            if (echoPrompt)
            {
                std::fputs("> ", stdout);
                std::fflush(stdout);
            }
            if (!std::getline(std::cin, line))
            {
                break; // EOF
            }
            if (line == "exit" || line == "quit")
            {
                break;
            }

            for (const auto& ins : ParseLine(line))
            {
                last = Execute(ctx, ins);
            }
        }
        return last;
    }
}
