// xy_engine.exe —— 运行期宿主。v1 只做四件事：
//   解析参数 → 载入程序 DLL → 调 xy_program_v1 → 卸载并返回退出码
// 模块装载、主循环、命令表都在后面几步加，这里刻意保持最小。

#include "Host.h"

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
        std::vector<std::string> args; // 含程序名，作为 argv[0]
        bool showHelp = false;
        bool showVersion = false;
    };

    void PrintUsage()
    {
        std::fputs(
            "xy_engine —— X_Y 运行期宿主\n"
            "\n"
            "用法:\n"
            "  xy_engine --program <path.dll> [参数...]\n"
            "  xy_engine --version | --help\n"
            "\n"
            "选项:\n"
            "  --program <dll>   要运行的程序 DLL（必须导出 xy_program_v1）\n"
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

    // ── 动态库 ----------------------------------------------------------------
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
    if (opt.showVersion)
    {
        std::printf("xy_engine  abi=%u  host_size=%zu\n", XY_ABI_VERSION, sizeof(XyHostV1));
        return 0;
    }

    const XyHostV1* host = xy::Host();

    Library lib;
    std::string err;
    if (!lib.Open(opt.program, err))
    {
        std::fprintf(stderr, "xy_engine: 载入 %s 失败: %s\n", opt.program.c_str(), err.c_str());
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

    const int code = entry(host, static_cast<int>(opt.args.size()), argvForProgram.data());
    return code;
}
