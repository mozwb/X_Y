// xy_engine.exe —— 运行期宿主。
//   v1：解析参数 → 装载模块（按声明顺序）→ 载入程序 DLL → 调入口
//       → 逆序卸载模块 → 返回退出码
// 命令表、主循环、store 都在后面加，这里刻意保持最小。

#include "Host.h"
#include "ModuleRegistry.h"

#include <xy_api.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace
{
    struct Options
    {
        std::string program;
        std::vector<std::string> modules; // 按给出顺序装载
        std::vector<std::string> args;    // 含程序名，作为 argv[0]
        bool showHelp = false;
        bool showVersion = false;
    };

    void PrintUsage()
    {
        std::fputs(
            "xy_engine —— X_Y 运行期宿主\n"
            "\n"
            "用法:\n"
            "  xy_engine --program <path.dll> [--module <m1.dll> ...] [参数...]\n"
            "  xy_engine --version | --help\n"
            "\n"
            "选项:\n"
            "  --program <dll>   要运行的程序 DLL（必须导出 xy_program_v1）\n"
            "  --module <dll>    装载一个模块 DLL（可重复；按给出顺序装载，退出时逆序卸载）\n"
            "  --version         打印 ABI 版本后退出\n"
            "  --help            打印本帮助\n",
            stderr);
    }

    bool ParseArgs(int argc, char** argv, Options& out)
    {
        for (int i = 1; i < argc; ++i)
        {
            const char* a = argv[i];
            if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0)
            {
                out.showHelp = true;
                return true;
            }
            if (std::strcmp(a, "--version") == 0)
            {
                out.showVersion = true;
                return true;
            }
            if (std::strcmp(a, "--program") == 0)
            {
                if (i + 1 >= argc)
                {
                    std::fputs("xy_engine: --program 缺少路径\n", stderr);
                    return false;
                }
                out.program = argv[++i];
                continue;
            }
            if (std::strcmp(a, "--module") == 0 || std::strcmp(a, "-m") == 0)
            {
                if (i + 1 >= argc)
                {
                    std::fputs("xy_engine: --module 缺少路径\n", stderr);
                    return false;
                }
                out.modules.emplace_back(argv[++i]);
                continue;
            }
            // 第一个非选项起，剩下的全部交给程序
            out.args.emplace_back(a);
            for (++i; i < argc; ++i)
            {
                out.args.emplace_back(argv[i]);
            }
            break;
        }

        if (out.program.empty())
        {
            std::fputs("xy_engine: 缺少 --program\n", stderr);
            return false;
        }
        if (out.args.empty())
        {
            // argv[0] 用程序文件名，方便程序自报家门
            const size_t slash = out.program.find_last_of("/\\");
            out.args.emplace_back(slash == std::string::npos ? out.program
                                                            : out.program.substr(slash + 1));
        }
        return true;
    }

    // ── 动态库（程序用；模块走 ModuleRegistry）────────────────────────────────
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
        return 64; // EX_USAGE
    }
    if (opt.showHelp)
    {
        PrintUsage();
        return 0;
    }

    xy::EngineContext ctx;
    const XyHostV1* host = xy::BuildHost(ctx);

    if (opt.showVersion)
    {
        std::printf("xy_engine  abi=%u  host_size=%zu\n", XY_ABI_VERSION, sizeof(XyHostV1));
        return 0;
    }

    // 内核自己的服务放进去，模块/程序可以用同一个查找路径拿到
    ctx.services.AddBuiltin("xy.host", const_cast<XyHostV1*>(host));

    int status = 0;

    {
        xy::ModuleRegistry modules(ctx);
        for (const auto& m : opt.modules)
        {
            if (!modules.Load(m))
            {
                // 已在 Load 内部报错；继续跑程序没有意义
                return 67;
            }
        }

        Library lib;
        std::string err;
        if (!lib.Open(opt.program, err))
        {
            std::fprintf(stderr, "xy_engine: 载入 %s 失败: %s\n",
                         opt.program.c_str(), err.c_str());
            return 65;
        }

        auto entry = reinterpret_cast<xy_program_v1_fn>(lib.Symbol(XY_PROGRAM_ENTRY));
        if (!entry)
        {
            std::fprintf(stderr, "xy_engine: %s 没有导出 %s\n",
                         opt.program.c_str(), XY_PROGRAM_ENTRY);
            return 66;
        }

        std::vector<const char*> argvForProgram;
        argvForProgram.reserve(opt.args.size() + 1);
        for (const auto& s : opt.args)
        {
            argvForProgram.push_back(s.c_str());
        }
        argvForProgram.push_back(nullptr);

        status = entry(host, static_cast<int>(opt.args.size()), argvForProgram.data());

        // 程序对象先析构（lib 在此作用域末尾才 FreeLibrary），
        // 之后才轮到模块逆序卸载 —— 顺序反了就是在给自己埋悬垂指针。
    }

    return status;
}
