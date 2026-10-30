#include "Shell.h"

#include "CommandTable.h"
#include "Host.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace X_Y
{
    namespace
    {
        // 命令执行期间把 currentModule 标成命令 owner，退出恢复。
        class OwnerScope
        {
        public:
            OwnerScope(std::string& slot, std::string owner)
                : slot_(slot)
                , previous_(std::move(slot_))
            {
                slot_ = std::move(owner);
            }

            ~OwnerScope() { slot_ = std::move(previous_); }

            OwnerScope(const OwnerScope&) = delete;
            OwnerScope& operator=(const OwnerScope&) = delete;

        private:
            std::string& slot_;
            std::string previous_;
        };

        bool IsSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        // 规则对齐 Modules/Terminal 的 Command::Parse：
        // 空白分隔；单引号原样；双引号只认 \" 和 \\；
        // 引号外 \x 转义成 x；相邻段拼成一个参数。
        bool Tokenize(const std::string& line,
                      std::vector<std::string>& tokens,
                      std::string& error)
        {
            tokens.clear();
            error.clear();

            std::string token;
            bool hasToken = false;

            auto finish = [&]()
            {
                if (!hasToken)
                {
                    return;
                }
                tokens.push_back(std::move(token));
                token.clear();
                hasToken = false;
            };

            std::size_t i = 0;
            while (i < line.size())
            {
                const char c = line[i];

                if (IsSpace(c))
                {
                    finish();
                    ++i;
                    continue;
                }

                if (c == '\'')
                {
                    hasToken = true;
                    ++i;
                    const std::size_t start = i;
                    while (i < line.size() && line[i] != '\'')
                    {
                        ++i;
                    }
                    if (i >= line.size())
                    {
                        error = "单引号没有闭合";
                        tokens.clear();
                        return false;
                    }
                    token.append(line, start, i - start);
                    ++i;
                    continue;
                }

                if (c == '"')
                {
                    hasToken = true;
                    ++i;
                    bool closed = false;

                    while (i < line.size())
                    {
                        const char ch = line[i];
                        if (ch == '"')
                        {
                            ++i;
                            closed = true;
                            break;
                        }
                        if (ch == '\\' && i + 1 < line.size())
                        {
                            const char next = line[i + 1];
                            if (next == '"' || next == '\\')
                            {
                                token.push_back(next);
                                i += 2;
                                continue;
                            }
                        }

                        token.push_back(ch);
                        ++i;
                    }

                    if (!closed)
                    {
                        error = "双引号没有闭合";
                        tokens.clear();
                        return false;
                    }
                    continue;
                }

                if (c == '\\')
                {
                    if (i + 1 >= line.size())
                    {
                        error = "结尾不能是单独的反斜杠";
                        tokens.clear();
                        return false;
                    }
                    hasToken = true;
                    token.push_back(line[i + 1]);
                    i += 2;
                    continue;
                }

                hasToken = true;
                token.push_back(c);
                ++i;
            }

            finish();
            return true;
        }

        int ExecuteTokens(EngineContext& ctx, const std::vector<std::string>& tokens)
        {
            if (tokens.empty())
            {
                return 0;
            }

            std::size_t first = 0;
            if (tokens[0] == "run")
            {
                first = 1;
                if (tokens.size() == 1)
                {
                    std::fprintf(stderr, "run 后面缺少命令名\n");
                    return 2;
                }
            }

            const std::string& name = tokens[first];
            const XyCommandV1* cmd = ctx.commands.Find(name);
            if (cmd == nullptr)
            {
                std::fprintf(stderr, "没有命令 '%s'（用 run help 看全部）\n", name.c_str());
                return 3;
            }

            std::vector<const char*> argv;
            argv.reserve(tokens.size() - first + 1);
            for (std::size_t i = first; i < tokens.size(); ++i)
            {
                argv.push_back(tokens[i].c_str());
            }
            argv.push_back(nullptr);

            OwnerScope scope(ctx.currentModule, ctx.commands.OwnerOf(name));
            return cmd->invoke(cmd->self,
                               static_cast<int>(tokens.size() - first),
                               argv.data());
        }

        bool ParseExitCode(const std::string& text, int& code)
        {
            char* end = nullptr;
            const long value = std::strtol(text.c_str(), &end, 10);
            if (end == text.c_str() || *end != '\0' || value < 0 || value > 255)
            {
                return false;
            }
            code = static_cast<int>(value);
            return true;
        }
    }

    int ExecuteArgs(EngineContext& ctx, const std::vector<std::string>& args)
    {
        return ExecuteTokens(ctx, args);
    }

    int ExecuteLine(EngineContext& ctx, const std::string& line)
    {
        std::vector<std::string> tokens;
        std::string error;
        if (!Tokenize(line, tokens, error))
        {
            std::fprintf(stderr, "解析失败: %s\n", error.c_str());
            return 2;
        }
        return ExecuteTokens(ctx, tokens);
    }

    int RunShell(EngineContext& ctx, bool prompt)
    {
        int last = 0;
        std::string line;

        for (;;)
        {
            if (prompt && ctx.interactive)
            {
                std::fputs("> ", stdout);
                std::fflush(stdout);
            }

            if (!std::getline(std::cin, line))
            {
                break;
            }

            std::vector<std::string> tokens;
            std::string error;
            if (!Tokenize(line, tokens, error))
            {
                std::fprintf(stderr, "解析失败: %s\n", error.c_str());
                last = 2;
                continue;
            }

            if (tokens.empty())
            {
                continue;
            }

            if (tokens[0] == "exit" || tokens[0] == "quit")
            {
                if (tokens.size() > 2)
                {
                    std::fprintf(stderr, "用法: %s [退出码]\n", tokens[0].c_str());
                    last = 2;
                    continue;
                }
                if (tokens.size() == 2)
                {
                    int code = 0;
                    if (!ParseExitCode(tokens[1], code))
                    {
                        std::fprintf(stderr, "退出码必须是 0-255 的整数\n");
                        last = 2;
                        continue;
                    }
                    return code;
                }
                return last;
            }

            last = ExecuteTokens(ctx, tokens);
        }

        return last;
    }
}
