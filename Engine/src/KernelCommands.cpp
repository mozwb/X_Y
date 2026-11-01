#include "KernelCommands.h"

#include "CommandTable.h"
#include "EngineContainer.h"
#include "Host.h"
#include "ModuleRegistry.h"
#include "Platform/Library.h"
#include "Platform/Process.h"
#include "Shell.h"

#include <Memory/Memory.h>
#include <xy_api.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace X_Y
{
    namespace
    {
        Engine* Ctx(void* self)
        {
            return static_cast<Engine*>(self);
        }

        std::string BaseName(const std::string& path)
        {
            const std::size_t slash = path.find_last_of("/\\");
            std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
            const std::size_t dot = name.find_last_of('.');
            if (dot != std::string::npos)
            {
                name.erase(dot);
            }
            return name;
        }

        // 一次性微程序也要有归属：它注册的服务按程序名记，跑完清掉。
        class ScopedOwner
        {
        public:
            ScopedOwner(std::string& slot, std::string owner)
                : slot_(slot)
                , previous_(std::move(slot_))
            {
                slot_ = std::move(owner);
            }

            ~ScopedOwner() { slot_ = std::move(previous_); }

            ScopedOwner(const ScopedOwner&) = delete;
            ScopedOwner& operator=(const ScopedOwner&) = delete;

        private:
            std::string& slot_;
            std::string previous_;
        };

        // 一次性程序执行期间禁止 add_service；退出自动恢复。
        class ScopedFlag
        {
        public:
            ScopedFlag(bool& flag, bool value)
                : flag_(flag)
                , previous_(flag)
            {
                flag_ = value;
            }

            ~ScopedFlag() { flag_ = previous_; }

            ScopedFlag(const ScopedFlag&) = delete;
            ScopedFlag& operator=(const ScopedFlag&) = delete;

        private:
            bool& flag_;
            bool previous_;
        };

        // ── help / commands ───────────────────────────────────────────────────
        int CmdCommands(void* self, int, const char* const*)
        {
            auto* ctx = Ctx(self);
            const auto names = ctx->commands.Names();
            for (const auto& name : names)
            {
                const XyCommandV1* cmd = ctx->commands.Find(name);
                std::printf("  %-16s %s\n", name.c_str(),
                            cmd != nullptr && cmd->help != nullptr ? cmd->help : "");
            }
            return 0;
        }

        int CmdHelp(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);

            if (argc >= 2)
            {
                const XyCommandV1* cmd = ctx->commands.Find(argv[1]);
                if (cmd == nullptr)
                {
                    std::fprintf(stderr, "没有命令 '%s'\n", argv[1]);
                    return 3;
                }
                std::printf("%s\n  %s\n", cmd->name,
                            cmd->help != nullptr ? cmd->help : "");
                return 0;
            }

            std::printf("内核只认一个动词: run <命令> [参数...]\n\n命令列表:\n");
            return CmdCommands(self, 0, nullptr);
        }

        // ── modules / load / unload ───────────────────────────────────────────
        int CmdModules(void* self, int, const char* const*)
        {
            auto* ctx = Ctx(self);
            if (ctx->modules == nullptr)
            {
                std::fprintf(stderr, "内核没有挂 ModuleRegistry\n");
                return 3;
            }

            const auto modules = ctx->modules->List();
            if (modules.empty())
            {
                std::printf("没有装载模块\n");
                return 0;
            }

            for (const auto& module : modules)
            {
                std::printf("  %-12s %-8s commands=%llu  %s\n",
                            module.name.c_str(),
                            module.hasProgram ? "program" : "-",
                            static_cast<unsigned long long>(module.commands.size()),
                            module.path.c_str());
            }
            return 0;
        }

        // ── engine container ─────────────────────────────────────────────────
        int CmdEngine(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);

            if (ctx->container == nullptr)
            {
                std::fprintf(stderr, "内核没有挂 EngineContainer\n");
                return 3;
            }
            if (argc < 2)
            {
                std::fprintf(stderr,
                             "用法: engine list | create <name> | destroy <name> | "
                             "load <name> <dll> | unload <name> <module> | "
                             "run <name> <命令...> | services <name> | shared | "
                             "share <name> <service> | unshare <service> | "
                             "share-command <name> <command> | unshare-command <command> | "
                             "shared-commands\n");
                return 2;
            }

            if (std::strcmp(argv[1], "list") == 0)
            {
                const auto engines = ctx->container->List();
                std::printf("%-16s %8s %8s\n", "NAME", "MODULES", "COMMANDS");
                for (const auto& engine : engines)
                {
                    std::printf("%-16s %8llu %8llu\n",
                                engine.name.c_str(),
                                static_cast<unsigned long long>(engine.modules),
                                static_cast<unsigned long long>(engine.commands));
                }
                return 0;
            }

            if (std::strcmp(argv[1], "services") == 0)
            {
                if (argc < 3)
                {
                    std::fprintf(stderr, "用法: engine services <name>\n");
                    return 2;
                }

                Engine* target = ctx->container->Find(argv[2]);
                if (target == nullptr)
                {
                    std::fprintf(stderr, "engine services: 没有 engine %s\n", argv[2]);
                    return 3;
                }

                const auto names = target->services.Names();
                if (names.empty())
                {
                    std::printf("engine %s 没有本地服务\n", argv[2]);
                    return 0;
                }

                std::printf("%-20s %s\n", "SERVICE", "OWNER");
                for (const auto& name : names)
                {
                    std::printf("%-20s %s\n",
                                name.c_str(),
                                target->services.OwnerOf(name).c_str());
                }
                return 0;
            }

            if (std::strcmp(argv[1], "shared") == 0)
            {
                const auto shared = ctx->container->SharedList();
                if (shared.empty())
                {
                    std::printf("容器没有共享服务\n");
                    return 0;
                }

                std::printf("%-20s %s\n", "SERVICE", "PROVIDER");
                for (const auto& item : shared)
                {
                    std::printf("%-20s %s\n",
                                item.service.c_str(),
                                item.provider.c_str());
                }
                return 0;
            }

            if (std::strcmp(argv[1], "create") == 0)
            {
                if (argc < 3)
                {
                    std::fprintf(stderr, "用法: engine create <name>\n");
                    return 2;
                }

                std::string error;
                Engine* created = ctx->container->Create(argv[2], error);
                if (created == nullptr)
                {
                    std::fprintf(stderr, "engine create: %s\n", error.c_str());
                    return 3;
                }
                std::printf("created engine %s\n", created->name.c_str());
                return 0;
            }

            if (std::strcmp(argv[1], "destroy") == 0)
            {
                if (argc < 3)
                {
                    std::fprintf(stderr, "用法: engine destroy <name>\n");
                    return 2;
                }

                std::string error;
                if (!ctx->container->Destroy(argv[2], error))
                {
                    std::fprintf(stderr, "engine destroy: %s\n", error.c_str());
                    return 3;
                }
                std::printf("destroyed engine %s\n", argv[2]);
                return 0;
            }

            if (std::strcmp(argv[1], "load") == 0)
            {
                if (argc < 4)
                {
                    std::fprintf(stderr, "用法: engine load <name> <dll>\n");
                    return 2;
                }

                Engine* target = ctx->container->Find(argv[2]);
                if (target == nullptr)
                {
                    std::fprintf(stderr, "engine load: 没有 engine %s\n", argv[2]);
                    return 3;
                }
                if (target->modules == nullptr)
                {
                    std::fprintf(stderr, "engine %s 没有 ModuleRegistry\n", argv[2]);
                    return 3;
                }

                std::string error;
                if (!target->modules->Load(argv[3], error))
                {
                    std::fprintf(stderr, "engine load: %s\n", error.c_str());
                    return 3;
                }
                std::printf("engine %s: loaded %s\n",
                            argv[2], BaseName(argv[3]).c_str());
                return 0;
            }

            if (std::strcmp(argv[1], "unload") == 0)
            {
                if (argc < 4)
                {
                    std::fprintf(stderr, "用法: engine unload <name> <module>\n");
                    return 2;
                }

                Engine* target = ctx->container->Find(argv[2]);
                if (target == nullptr)
                {
                    std::fprintf(stderr, "engine unload: 没有 engine %s\n", argv[2]);
                    return 3;
                }
                if (target->modules == nullptr)
                {
                    std::fprintf(stderr, "engine %s 没有 ModuleRegistry\n", argv[2]);
                    return 3;
                }
                if (ctx->container->HasSharedServices(target))
                {
                    std::fprintf(stderr,
                                 "engine %s 还有共享服务，先 engine unshare <service>\n",
                                 argv[2]);
                    return 3;
                }

                int leaks = 0;
                std::string error;
                if (!target->modules->Unload(argv[3], leaks, error))
                {
                    std::fprintf(stderr, "engine unload: %s\n", error.c_str());
                    return 3;
                }
                if (leaks > 0)
                {
                    std::fprintf(stderr,
                                 "警告: 卸载 %s 时有 %d 个服务未反注册（已强制移除）\n",
                                 argv[3], leaks);
                }
                std::printf("engine %s: unloaded %s\n", argv[2], argv[3]);
                return 0;
            }

            if (std::strcmp(argv[1], "run") == 0)
            {
                if (argc < 4)
                {
                    std::fprintf(stderr, "用法: engine run <name> <命令...>\n");
                    return 2;
                }

                Engine* target = ctx->container->Find(argv[2]);
                if (target == nullptr)
                {
                    std::fprintf(stderr, "engine run: 没有 engine %s\n", argv[2]);
                    return 3;
                }

                std::vector<std::string> args;
                args.reserve(static_cast<std::size_t>(argc - 3));
                for (int i = 3; i < argc; ++i)
                {
                    args.emplace_back(argv[i]);
                }
                return ExecuteArgs(*target, args);
            }

            if (std::strcmp(argv[1], "share") == 0)
            {
                if (argc < 4)
                {
                    std::fprintf(stderr, "用法: engine share <name> <service>\n");
                    return 2;
                }

                std::string error;
                if (!ctx->container->Share(argv[2], argv[3], error))
                {
                    std::fprintf(stderr, "engine share: %s\n", error.c_str());
                    return 3;
                }
                std::printf("shared service %s from engine %s\n", argv[3], argv[2]);
                return 0;
            }

            if (std::strcmp(argv[1], "unshare") == 0)
            {
                if (argc < 3)
                {
                    std::fprintf(stderr, "用法: engine unshare <service>\n");
                    return 2;
                }

                std::string error;
                if (!ctx->container->Unshare(argv[2], error))
                {
                    std::fprintf(stderr, "engine unshare: %s\n", error.c_str());
                    return 3;
                }
                std::printf("unshared service %s\n", argv[2]);
                return 0;
            }

            if (std::strcmp(argv[1], "share-command") == 0)
            {
                if (argc < 4)
                {
                    std::fprintf(stderr, "用法: engine share-command <name> <command>\n");
                    return 2;
                }

                std::string error;
                if (!ctx->container->ShareCommand(argv[2], argv[3], error))
                {
                    std::fprintf(stderr, "engine share-command: %s\n", error.c_str());
                    return 3;
                }
                std::printf("shared command %s from engine %s\n", argv[3], argv[2]);
                return 0;
            }

            if (std::strcmp(argv[1], "unshare-command") == 0)
            {
                if (argc < 3)
                {
                    std::fprintf(stderr, "用法: engine unshare-command <command>\n");
                    return 2;
                }

                std::string error;
                if (!ctx->container->UnshareCommand(argv[2], error))
                {
                    std::fprintf(stderr, "engine unshare-command: %s\n", error.c_str());
                    return 3;
                }
                std::printf("unshared command %s\n", argv[2]);
                return 0;
            }

            if (std::strcmp(argv[1], "shared-commands") == 0)
            {
                const auto commands = ctx->container->SharedCommandList();
                if (commands.empty())
                {
                    std::printf("容器没有共享命令\n");
                    return 0;
                }

                std::printf("%-20s %s\n", "COMMAND", "PROVIDER");
                for (const auto& item : commands)
                {
                    std::printf("%-20s %s\n",
                                item.command.c_str(),
                                item.provider.c_str());
                }
                return 0;
            }

            std::fprintf(stderr, "engine: 未知子命令 '%s'\n", argv[1]);
            return 2;
        }

        int CmdLoad(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);
            if (argc < 2)
            {
                std::fprintf(stderr, "用法: load <dll>\n");
                return 2;
            }
            if (ctx->modules == nullptr)
            {
                std::fprintf(stderr, "内核没有挂 ModuleRegistry\n");
                return 3;
            }

            std::string error;
            if (!ctx->modules->Load(argv[1], error))
            {
                std::fprintf(stderr, "装载失败: %s\n", error.c_str());
                return 3;
            }

            std::printf("loaded %s\n", BaseName(argv[1]).c_str());
            return 0;
        }

        int CmdUnload(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);
            if (argc < 2)
            {
                std::fprintf(stderr, "用法: unload <模块名>\n");
                return 2;
            }
            if (ctx->modules == nullptr)
            {
                std::fprintf(stderr, "内核没有挂 ModuleRegistry\n");
                return 3;
            }

            if (ctx->container != nullptr &&
                (ctx->container->HasSharedServices(ctx) ||
                 ctx->container->HasSharedCommands(ctx)))
            {
                std::fprintf(stderr,
                             "engine 还有共享服务/命令，先 unshare 再卸载模块\n");
                return 3;
            }

            int leaks = 0;
            std::string error;
            if (!ctx->modules->Unload(argv[1], leaks, error))
            {
                std::fprintf(stderr, "卸载失败: %s\n", error.c_str());
                return 3;
            }

            if (leaks > 0)
            {
                std::fprintf(stderr,
                             "警告: 卸载 %s 时有 %d 个服务未反注册（已强制移除）\n",
                             argv[1], leaks);
            }
            std::printf("unloaded %s\n", argv[1]);
            return 0;
        }

        // ── program：一次性跑微程序，不登记命令 ──────────────────────────────
        int CmdProgram(void* self, int argc, const char* const* argv)
        {
            auto* ctx = Ctx(self);
            if (argc < 2)
            {
                std::fprintf(stderr, "用法: program <dll> [参数...]\n");
                return 2;
            }

            const std::string path = argv[1];
            const std::string name = BaseName(path);

            Platform::Library library;
            std::string error;
            if (!library.Open(path, error))
            {
                std::fprintf(stderr, "打开失败: %s\n", error.c_str());
                return 3;
            }

            auto version = reinterpret_cast<xy_abi_version_v1_fn>(
                library.Symbol(XY_ABI_VERSION_ENTRY));
            if (version == nullptr)
            {
                std::fprintf(stderr, "%s 没有导出 %s\n",
                             name.c_str(), XY_ABI_VERSION_ENTRY);
                return 4;
            }

            const uint32_t abi = version();
            if (abi != XY_ABI_VERSION)
            {
                std::fprintf(stderr, "ABI 版本不匹配: %s 是 %u，内核是 %u\n",
                             name.c_str(), abi, XY_ABI_VERSION);
                return 4;
            }

            auto entry = reinterpret_cast<xy_program_v1_fn>(
                library.Symbol(XY_PROGRAM_ENTRY));
            if (entry == nullptr)
            {
                std::fprintf(stderr, "%s 没有导出 %s\n",
                             name.c_str(), XY_PROGRAM_ENTRY);
                return 5;
            }

            std::vector<const char*> programArgv;
            programArgv.reserve(static_cast<std::size_t>(argc));
            programArgv.push_back(name.c_str());
            for (int i = 2; i < argc; ++i)
            {
                programArgv.push_back(argv[i]);
            }
            programArgv.push_back(nullptr);

            int rc = 0;
            {
                ScopedFlag oneShot(ctx->oneShot, true);
                ScopedOwner scope(ctx->currentModule, name);
                rc = entry(&ctx->host,
                           static_cast<int>(programArgv.size() - 1),
                           programArgv.data());
            }

            // oneShot 期间 add_service 已经会被拒绝；这里只是兜底，
            // 防旧 DLL 或绕过 host 的注册。真摘到东西说明契约被破坏了。
            int leaks = 0;
            for (const auto& service : ctx->services.OwnedBy(name))
            {
                ctx->services.Remove(service);
                ++leaks;
            }
            if (leaks > 0)
            {
                std::fprintf(stderr,
                             "警告: 微程序 %s 留下了 %d 个服务（已移除）\n",
                             name.c_str(), leaks);
            }

            return rc;
        }

        // ── exec：外部进程 ─────────────────────────────────────────────────────
        int CmdExec(void* self, int argc, const char* const* argv)
        {
            (void)self;

            if (argc < 2)
            {
                std::fprintf(stderr, "用法: exec [--capture] <exe> [参数...]\n");
                return 2;
            }

            bool capture = false;
            int index = 1;
            if (std::strcmp(argv[index], "--capture") == 0)
            {
                capture = true;
                ++index;
            }
            if (index >= argc)
            {
                std::fprintf(stderr, "用法: exec [--capture] <exe> [参数...]\n");
                return 2;
            }

            const std::string program = argv[index];
            std::vector<std::string> arguments;
            for (int i = index + 1; i < argc; ++i)
            {
                arguments.emplace_back(argv[i]);
            }

            Process::Options options;
            options.Capture = capture;
            const Process::Result result = Process::Run(program, arguments, options);

            if (!result.Started)
            {
                std::fprintf(stderr, "启动失败: %s\n", result.Error.c_str());
                return 3;
            }
            if (!result.Error.empty())
            {
                std::fprintf(stderr, "等待失败: %s\n", result.Error.c_str());
                return 3;
            }

            if (capture && !result.Output.empty())
            {
                std::fwrite(result.Output.data(), 1, result.Output.size(), stdout);
                std::fflush(stdout);
            }

            return result.ExitCode;
        }

        // ── mem / abi ─────────────────────────────────────────────────────────
        int CmdMem(void* self, int, const char* const*)
        {            (void)self;
            const auto stats = Memory::Instance().stats();
            std::printf("门面: used=%llu peak=%llu live=%llu allocs=%llu frees=%llu overhead=%llu\n",
                        static_cast<unsigned long long>(stats.UsedBytes),
                        static_cast<unsigned long long>(stats.PeakBytes),
                        static_cast<unsigned long long>(stats.LiveBlocks),
                        static_cast<unsigned long long>(stats.TotalAllocs),
                        static_cast<unsigned long long>(stats.TotalFrees),
                        static_cast<unsigned long long>(stats.OverheadBytes));

            const auto process = Memory::processMemory();
            if (process.Valid)
            {
                std::printf("进程: rss=%llu peak_rss=%llu private=%llu\n",
                            static_cast<unsigned long long>(process.WorkingSetBytes),
                            static_cast<unsigned long long>(process.PeakWorkingSetBytes),
                            static_cast<unsigned long long>(process.PrivateBytes));
            }
            return 0;
        }

        int CmdAbi(void* self, int, const char* const*)
        {
            (void)self;
            std::printf("abi=%u host=%llu bytes program=%s commands=%s\n",
                        static_cast<unsigned>(XY_ABI_VERSION),
                        static_cast<unsigned long long>(sizeof(XyHostV1)),
                        XY_PROGRAM_ENTRY, XY_COMMANDS_ENTRY);
            return 0;
        }

        // ── 内建命令对象 ──────────────────────────────────────────────────────
        XyCommandV1 kCommands = {
            sizeof(XyCommandV1), "commands", "列出所有命令", &CmdCommands, nullptr
        };
        XyCommandV1 kHelp = {
            sizeof(XyCommandV1), "help", "帮助：help 或 help <命令>", &CmdHelp, nullptr
        };
        XyCommandV1 kModules = {
            sizeof(XyCommandV1), "modules", "列出已装载模块", &CmdModules, nullptr
        };
        XyCommandV1 kLoad = {
            sizeof(XyCommandV1), "load", "装载 DLL 的命令/微程序", &CmdLoad, nullptr
        };
        XyCommandV1 kUnload = {
            sizeof(XyCommandV1), "unload", "按模块名卸载", &CmdUnload, nullptr
        };
        XyCommandV1 kProgram = {
            sizeof(XyCommandV1), "program", "一次性运行微程序 <dll> [参数...]", &CmdProgram, nullptr
        };
        XyCommandV1 kExec = {
            sizeof(XyCommandV1), "exec", "跑外部程序 [--capture] <exe> [参数...]", &CmdExec, nullptr
        };
        XyCommandV1 kEngine = {
            sizeof(XyCommandV1), "engine",
            "engine list/create/destroy/load/unload/run/services/shared/share/unshare/share-command/unshare-command/shared-commands",
            &CmdEngine, nullptr
        };
        XyCommandV1 kMem = {
            sizeof(XyCommandV1), "mem", "打印 Memory 统计", &CmdMem, nullptr
        };
        XyCommandV1 kAbi = {
            sizeof(XyCommandV1), "abi", "打印 ABI 版本与 host 大小", &CmdAbi, nullptr
        };
    }

    void RegisterKernelCommands(Engine& ctx)
    {
        XyCommandV1* const builtins[] = {
            &kCommands, &kHelp, &kModules, &kEngine, &kLoad,
            &kUnload, &kProgram, &kExec, &kMem, &kAbi,
        };

        for (XyCommandV1* cmd : builtins)
        {
            cmd->self = &ctx;
            if (!ctx.commands.Add(cmd, "engine"))
            {
                std::fprintf(stderr, "登记内建命令失败（重名？）: %s\n", cmd->name);
            }
        }
    }
}
