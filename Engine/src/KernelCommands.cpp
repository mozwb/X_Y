#include "KernelCommands.h"

#include "CommandTable.h"
#include "Host.h"

#include <xy_command.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace xy
{
    namespace
    {
        EngineContext* Ctx(void* self) { return static_cast<EngineContext*>(self); }

        // ── alloc <bytes> ───────────────────────────────────────────────────────
        int CmdAlloc(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);
            if (argc < 2)
            {
                std::fprintf(stderr, "用法: run alloc <字节数>\n");
                return 2;
            }
            const long n = std::strtol(argv[1], nullptr, 10);
            if (n <= 0)
            {
                std::fprintf(stderr, "alloc: 字节数必须为正\n");
                return 2;
            }

            void* p = ctx->host.alloc(ctx->host.user, static_cast<size_t>(n), 0);
            if (!p)
            {
                std::fprintf(stderr, "alloc: 失败\n");
                return 3;
            }
            ctx->blocks[p] = static_cast<size_t>(n);
            std::printf("0x%p  %ld bytes\n", p, n);
            return 0;
        }

        // ── free <hex 地址> ────────────────────────────────────────────────────
        int CmdFree(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);
            if (argc < 2)
            {
                std::fprintf(stderr, "用法: run free <地址>\n");
                return 2;
            }

            const char* s = argv[1];
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            {
                s += 2;
            }
            char* end = nullptr;
            const unsigned long long v = std::strtoull(s, &end, 16);
            if (!end || *end != '\0')
            {
                std::fprintf(stderr, "free: 地址格式不对（要 0x 开头的十六进制）\n");
                return 2;
            }

            void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(v));
            // 只认 alloc 登记过的地址：外部指针一律拒绝，避免两套堆混用
            const auto it = ctx->blocks.find(p);
            if (it == ctx->blocks.end())
            {
                std::fprintf(stderr, "free: 这个地址不是 alloc 发出的，拒绝释放\n");
                return 3;
            }
            const size_t n = it->second;
            ctx->blocks.erase(it);
            ctx->host.free(ctx->host.user, p);
            std::printf("freed  %zu bytes\n", n);
            return 0;
        }

        // ── mem ────────────────────────────────────────────────────────────────
        int CmdMem(void* self, int, const char* const*)
        {
            auto* ctx = Ctx(self);
            size_t total = 0;
            for (const auto& [p, n] : ctx->blocks)
            {
                total += n;
            }
            std::printf("块数 %zu，共 %zu 字节\n", ctx->blocks.size(), total);
            for (const auto& [p, n] : ctx->blocks)
            {
                std::printf("  0x%p  %zu\n", p, n);
            }
            return 0;
        }

        // ── commands ───────────────────────────────────────────────────────────
        int CmdCommands(void* self, int, const char* const*)
        {
            auto* ctx = Ctx(self);
            CommandTable table;
            table.Refresh(ctx->services);
            for (const auto& name : table.Names())
            {
                const auto* c = table.Find(name);
                std::printf("  %-12s %s\n", name.c_str(), c->help ? c->help : "");
            }
            return 0;
        }

        // ── help [命令] ────────────────────────────────────────────────────────
        int CmdHelp(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);
            CommandTable table;
            table.Refresh(ctx->services);

            if (argc >= 2)
            {
                const auto* c = table.Find(argv[1]);
                if (!c)
                {
                    std::fprintf(stderr, "没有命令 '%s'\n", argv[1]);
                    return 3;
                }
                std::printf("%s\n  %s\n", c->name, c->help ? c->help : "");
                return 0;
            }

            std::printf("内核只认一个动词: run <命令> [参数...]\n\n命令列表:\n");
            return CmdCommands(self, 0, nullptr);
        }

        XyCommandV1 kAlloc = {sizeof(XyCommandV1), "alloc", "分配一块内存，打印地址", &CmdAlloc, nullptr};
        XyCommandV1 kFree = {sizeof(XyCommandV1), "free", "释放 alloc 发出的地址", &CmdFree, nullptr};
        XyCommandV1 kMem = {sizeof(XyCommandV1), "mem", "列出当前分配", &CmdMem, nullptr};
        XyCommandV1 kCommands = {sizeof(XyCommandV1), "commands", "列出所有命令", &CmdCommands, nullptr};
        XyCommandV1 kHelp = {sizeof(XyCommandV1), "help", "帮助：help 或 help <命令>", &CmdHelp, nullptr};
    }

    void RegisterKernelCommands(EngineContext& ctx)
    {
        struct Entry
        {
            const char* service;
            XyCommandV1* cmd;
        };
        const Entry entries[] = {
            {XY_CMD_SERVICE_PREFIX "alloc", &kAlloc},
            {XY_CMD_SERVICE_PREFIX "free", &kFree},
            {XY_CMD_SERVICE_PREFIX "mem", &kMem},
            {XY_CMD_SERVICE_PREFIX "commands", &kCommands},
            {XY_CMD_SERVICE_PREFIX "help", &kHelp},
        };
        for (const auto& e : entries)
        {
            e.cmd->self = &ctx; // self 统一指向引擎上下文
            ctx.services.AddBuiltin(e.service, e.cmd);
        }
    }
}
