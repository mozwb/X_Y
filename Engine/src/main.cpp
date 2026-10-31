// X_Y.exe —— 运行期宿主。
//
// 四种入口：
//   X_Y                                  进入交互 shell
//   X_Y --module <dll>                   启动时装载模块（可重复）
//   X_Y --run <命令> [参数...]           执行一条命令
//   X_Y --program <dll> [参数...]        一次性运行微程序
//
// 内核只认一个动词：run <命令> [参数...]。
// --run / --program 只是把命令行参数直接交给命令表，不重新拼字符串。

#include "Host.h"
#include "KernelCommands.h"
#include "ModuleRegistry.h"
#include "Shell.h"

#include <Memory/Memory.h>
#include <xy_api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{
    enum class Mode
    {
        Shell,
        Run,
        Program,
        Help,
        Version
    };

    struct Options
    {
        Mode mode = Mode::Shell;
        std::string target;
        std::vector<std::string> args;
        std::vector<std::string> modules;
    };

    void PrintUsage(std::FILE* stream)
    {
        std::fputs(
            "X_Y —— X_Y 运行期宿主\n"
            "\n"
            "用法:\n"
            "  X_Y                                  进入交互 shell\n"
            "  X_Y --module <dll>                   启动时装载模块（可重复）\n"
            "  X_Y --run <命令> [参数...]           执行一条命令\n"
            "  X_Y --program <dll> [参数...]        一次性运行微程序\n"
            "  X_Y --version | --help\n"
            "\n"
            "内核只认一个动词: run <命令> [参数...]\n",
            stream);
    }

    bool TakeValue(int argc, char** argv, int& i, const char* what, std::string& out)
    {
        if (i + 1 >= argc)
        {
            std::fprintf(stderr, "X_Y: %s 缺少值\n", what);
            return false;
        }
        out = argv[++i];
        return true;
    }

    bool ParseArgs(int argc, char** argv, Options& out)
    {
        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];

            if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
            {
                out.mode = Mode::Help;
                return true;
            }
            if (std::strcmp(arg, "--version") == 0)
            {
                out.mode = Mode::Version;
                return true;
            }
            if (std::strcmp(arg, "--module") == 0 || std::strcmp(arg, "-m") == 0)
            {
                std::string value;
                if (!TakeValue(argc, argv, i, "--module", value))
                {
                    return false;
                }
                out.modules.push_back(std::move(value));
                continue;
            }
            if (std::strcmp(arg, "--run") == 0 || std::strcmp(arg, "--program") == 0)
            {
                out.mode = std::strcmp(arg, "--run") == 0 ? Mode::Run : Mode::Program;
                if (!TakeValue(argc, argv, i, arg, out.target))
                {
                    return false;
                }
                for (++i; i < argc; ++i)
                {
                    out.args.emplace_back(argv[i]);
                }
                break;
            }

            std::fprintf(stderr, "X_Y: 不认识的参数 '%s'\n", arg);
            return false;
        }
        return true;
    }

    // 固定布局：X_Y 在 bin/，pkg 默认装到 bin/../modules。
    // 通过环境变量交给 pkg（pkg 是 DLL，拿不到 argv[0]）。
    void SetDefaultModuleRoot(const char* argv0)
    {
        if (std::getenv("XY_MODULES") != nullptr)
        {
            return;
        }

        std::error_code ec;
        const std::filesystem::path exe = std::filesystem::absolute(argv0, ec);
        if (ec || exe.empty())
        {
            return;
        }

        const std::filesystem::path modules = exe.parent_path() / "../modules";
        std::filesystem::create_directories(modules, ec);

        std::error_code canonEc;
        const std::filesystem::path normalized =
            std::filesystem::weakly_canonical(modules, canonEc);
        const std::string text = (canonEc ? modules : normalized).string();

#if defined(_WIN32)
        _putenv_s("XY_MODULES", text.c_str());
#else
        setenv("XY_MODULES", text.c_str(), 0);
#endif
    }
}

int main(int argc, char** argv)
{
    Options opt;
    if (!ParseArgs(argc, argv, opt))
    {
        PrintUsage(stderr);
        return 64;
    }

    if (opt.mode == Mode::Help)
    {
        PrintUsage(stdout);
        return 0;
    }
    if (opt.mode == Mode::Version)
    {
        std::printf("X_Y  abi=%u  host=%llu bytes\n",
                    static_cast<unsigned>(XY_ABI_VERSION),
                    static_cast<unsigned long long>(sizeof(XyHostV1)));
        return 0;
    }

    SetDefaultModuleRoot(argv[0]);

    X_Y::EngineContext ctx;
    const XyHostV1* host = X_Y::BuildHost(ctx);

    // 启动期的分配不算进 mem 的“使用量”，但仍保留在进程统计里。
    X_Y::Memory::Instance().markBaseline();

    X_Y::ModuleRegistry modules(*host, ctx.services, ctx.commands, ctx.currentModule);
    ctx.modules = &modules;
    X_Y::RegisterKernelCommands(ctx);

    for (const auto& path : opt.modules)
    {
        std::string error;
        if (!modules.Load(path, error))
        {
            std::fprintf(stderr, "X_Y: 装载模块 %s 失败: %s\n",
                         path.c_str(), error.c_str());
            return 67;
        }
    }

    int status = 0;

    switch (opt.mode)
    {
    case Mode::Program:
    {
        std::vector<std::string> args;
        args.reserve(opt.args.size() + 2);
        args.push_back("program");
        args.push_back(opt.target);
        args.insert(args.end(), opt.args.begin(), opt.args.end());
        status = X_Y::ExecuteArgs(ctx, args);
        break;
    }

    case Mode::Run:
    {
        std::vector<std::string> args;
        args.reserve(opt.args.size() + 1);
        args.push_back(opt.target);
        args.insert(args.end(), opt.args.begin(), opt.args.end());
        status = X_Y::ExecuteArgs(ctx, args);
        break;
    }

    case Mode::Shell:
    default:
        ctx.interactive = true;
        status = X_Y::RunShell(ctx, true);
        break;
    }

    return status;
}
