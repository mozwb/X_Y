// X_Y.exe —— 运行期宿主。四种入口，共用同一个命令表和模块注册表：
//
//   --program <dll> [参数...]   跑一个微程序（dll，导出 xy_program_v1）
//   --exec    <exe> [参数...]   把可执行程序当子进程拉起（我们不管它之后怎么跑）
//   --run     <命令> [参数...]  执行一条命令（内核命令或 dll 命令）
//   （无）                      进入交互模式，逐行读 run 指令
//
// 内核只认一个动词：run <命令> [参数...]。解析器在 Shell 里，可替换。

#include "CommandTable.h"
#include "Host.h"
#include "KernelCommands.h"
#include "ModuleRegistry.h"
#include "Shell.h"

#include <xy_api.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace
{
    enum class Mode
    {
        Shell,
        Program,
        Exec,
        Run,
        Help,
        Version
    };

    struct Options
    {
        Mode mode = Mode::Shell;
        std::string target;               // program dll / exe / 命令名
        std::vector<std::string> args;    // 交给目标的参数
        std::vector<std::string> modules; // 按给出顺序装载
    };

    void PrintUsage()
    {
        std::fputs(
            "X_Y —— X_Y 运行期宿主\n"
            "\n"
            "用法:\n"
            "  X_Y                                 进入交互模式\n"
            "  X_Y --program <dll>  [参数...]      跑微程序\n"
            "  X_Y --exec    <exe>  [参数...]      拉起可执行程序\n"
            "  X_Y --run     <命令> [参数...]      执行一条命令\n"
            "  X_Y --module  <dll>                 装载模块（可重复，先装先卸）\n"
            "  X_Y --version | --help\n"
            "\n"
            "内核只认一个动词: run <命令> [参数...]\n",
            stderr);
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
            const char* a = argv[i];
            if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0)
            {
                out.mode = Mode::Help;
                return true;
            }
            if (std::strcmp(a, "--version") == 0)
            {
                out.mode = Mode::Version;
                return true;
            }
            if (std::strcmp(a, "--module") == 0 || std::strcmp(a, "-m") == 0)
            {
                std::string v;
                if (!TakeValue(argc, argv, i, "--module", v))
                {
                    return false;
                }
                out.modules.push_back(v);
                continue;
            }
            if (std::strcmp(a, "--program") == 0 || std::strcmp(a, "--exec") == 0 ||
                std::strcmp(a, "--run") == 0)
            {
                out.mode = std::strcmp(a, "--program") == 0 ? Mode::Program
                         : std::strcmp(a, "--exec") == 0    ? Mode::Exec
                                                            : Mode::Run;
                if (!TakeValue(argc, argv, i, a, out.target))
                {
                    return false;
                }
                // 后面剩下的一律交给目标
                for (++i; i < argc; ++i)
                {
                    out.args.emplace_back(argv[i]);
                }
                break;
            }

            std::fprintf(stderr, "X_Y: 不认识的参数 '%s'\n", a);
            return false;
        }
        return true;
    }

    // ── 可执行程序：当子进程拉起，等它退出并原样返回它的退出码 ────────────────
#if defined(_WIN32)
    // 只在必要时加引号：无条件加引号会把 `cmd /c exit 7` 这种拆错
    std::string QuoteIfNeeded(const std::string& s)
    {
        if (s.find_first_of(" \t\"") == std::string::npos)
        {
            return s;
        }
        std::string out = "\"";
        for (const char c : s)
        {
            if (c == '"')
            {
                out += "\\\"";
            }
            else
            {
                out += c;
            }
        }
        out += "\"";
        return out;
    }
#endif

    int RunChild(const std::string& exe, const std::vector<std::string>& args)
    {
#if defined(_WIN32)
        std::string cmdline = QuoteIfNeeded(exe);
        for (const auto& s : args)
        {
            cmdline += " ";
            cmdline += QuoteIfNeeded(s);
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<char> buf(cmdline.begin(), cmdline.end());
        buf.push_back('\0');

        if (!::CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE, 0,
                              nullptr, nullptr, &si, &pi))
        {
            std::fprintf(stderr, "X_Y: 拉起 %s 失败，错误码 %lu\n",
                         exe.c_str(), ::GetLastError());
            return 68;
        }
        ::WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        ::GetExitCodeProcess(pi.hProcess, &code);
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
        return static_cast<int>(code);
#else
        const pid_t pid = ::fork();
        if (pid < 0)
        {
            std::fputs("X_Y: fork 失败\n", stderr);
            return 68;
        }
        if (pid == 0)
        {
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(exe.c_str()));
            for (const auto& s : args)
            {
                argv.push_back(const_cast<char*>(s.c_str()));
            }
            argv.push_back(nullptr);
            ::execv(exe.c_str(), argv.data());
            ::_exit(127);
        }
        int status = 0;
        ::waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 68;
#endif
    }

    // ── 微程序：载入 dll，调 xy_program_v1 ────────────────────────────────────
#if defined(_WIN32)
    using NativeHandle = ::HMODULE;
#else
    using NativeHandle = void*;
#endif

    class Library
    {
    public:
        ~Library() { Close(); }

        bool Open(const std::string& path, std::string& err)
        {
            Close();
#if defined(_WIN32)
            handle_ = ::LoadLibraryA(path.c_str());
            if (!handle_)
            {
                err = "LoadLibrary 失败，错误码 " + std::to_string(::GetLastError());
                return false;
            }
#else
            handle_ = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle_)
            {
                const char* e = ::dlerror();
                err = e ? e : "dlopen 失败";
                return false;
            }
#endif
            return true;
        }

        void Close()
        {
            if (!handle_)
            {
                return;
            }
#if defined(_WIN32)
            ::FreeLibrary(handle_);
#else
            ::dlclose(handle_);
#endif
            handle_ = nullptr;
        }

        void* Symbol(const char* name) const
        {
            if (!handle_)
            {
                return nullptr;
            }
#if defined(_WIN32)
            return reinterpret_cast<void*>(::GetProcAddress(handle_, name));
#else
            return ::dlsym(handle_, name);
#endif
        }

    private:
        NativeHandle handle_ = nullptr;
    };
}

int main(int argc, char** argv)
{
    Options opt;
    if (!ParseArgs(argc, argv, opt))
    {
        PrintUsage();
        return 64;
    }
    if (opt.mode == Mode::Help)
    {
        PrintUsage();
        return 0;
    }
    if (opt.mode == Mode::Version)
    {
        std::printf("X_Y  abi=%u  host_size=%zu\n", XY_ABI_VERSION, sizeof(XyHostV1));
        return 0;
    }

    X_Y::EngineContext ctx;
    const XyHostV1* host = X_Y::BuildHost(ctx);
    ctx.services.AddBuiltin("xy.host", const_cast<XyHostV1*>(host));
    X_Y::RegisterKernelCommands(ctx);

    int status = 0;
    {
        X_Y::ModuleRegistry modules(ctx);
        for (const auto& m : opt.modules)
        {
            if (!modules.Load(m))
            {
                return 67;
            }
        }

        switch (opt.mode)
        {
        case Mode::Exec:
            status = RunChild(opt.target, opt.args);
            break;

        case Mode::Run:
        {
            X_Y::Instruction ins;
            ins.verb = "run";
            ins.args.push_back(opt.target);
            ins.args.insert(ins.args.end(), opt.args.begin(), opt.args.end());
            status = X_Y::Execute(ctx, ins);
            break;
        }

        case Mode::Program:
        {
            Library lib;
            std::string err;
            if (!lib.Open(opt.target, err))
            {
                std::fprintf(stderr, "X_Y: 载入 %s 失败: %s\n",
                             opt.target.c_str(), err.c_str());
                return 65;
            }
            auto entry = reinterpret_cast<xy_program_v1_fn>(lib.Symbol(XY_PROGRAM_ENTRY));
            if (!entry)
            {
                std::fprintf(stderr, "X_Y: %s 没有导出 %s\n",
                             opt.target.c_str(), XY_PROGRAM_ENTRY);
                return 66;
            }

            std::vector<std::string> args = opt.args;
            if (args.empty())
            {
                const size_t slash = opt.target.find_last_of("/\\");
                args.push_back(slash == std::string::npos ? opt.target
                                                          : opt.target.substr(slash + 1));
            }
            std::vector<const char*> argvForProgram;
            for (const auto& s : args)
            {
                argvForProgram.push_back(s.c_str());
            }
            argvForProgram.push_back(nullptr);

            status = entry(host, static_cast<int>(args.size()), argvForProgram.data());
            // 程序对象先析构，之后才轮到模块逆序卸载
            break;
        }

        case Mode::Shell:
        default:
            ctx.interactive = true;
            status = X_Y::RunShell(ctx, true);
            break;
        }
    }

    return status;
}
