// xy —— X_Y 的最小构建器 + 仓库安装入口（v1）。
//
// 两条命令：
//   xy build   <源目录> [--out <目录>] [--include <Engine/include>] [--cxx <编译器>]
//   xy install <name> [version] [--repo <仓库>] [--root <安装根>] [--build] [--force]
//
// install 的流程：
//   1. 在 repo 里递归找 package.xy，按 name + version 选包
//   2. 有二进制（file 存在）→ 直接交给 pkg install
//   3. 只有源码 / --build → 先 xy build 到 .cache/out，再交给 pkg install
//
// xy 自己不写 Modules；收包永远由 pkg 做。

#include "Platform/Process.h"

#include <xy_api.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    struct Manifest
    {
        std::string name;
        std::string version;
        std::string kind;
        std::string abi;
        std::string file;
        std::string source;
    };

    struct RepoEntry
    {
        fs::path directory;
        Manifest manifest;
        bool binary = false;
        bool source = false;
    };

    std::string_view Trim(std::string_view text)
    {
        while (!text.empty() &&
               std::isspace(static_cast<unsigned char>(text.front())) != 0)
        {
            text.remove_prefix(1);
        }
        while (!text.empty() &&
               std::isspace(static_cast<unsigned char>(text.back())) != 0)
        {
            text.remove_suffix(1);
        }
        return text;
    }

    std::string ToString(std::string_view text)
    {
        return std::string(text.data(), text.size());
    }

    bool IsSafeComponent(const std::string& text)
    {
        if (text.empty() || text == "." || text == "..")
        {
            return false;
        }
        for (const char c : text)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc) == 0 &&
                c != '.' && c != '-' && c != '_' && c != '+')
            {
                return false;
            }
        }
        return true;
    }

    bool IsSafeRelativePath(const std::string& text)
    {
        const fs::path path(text);
        if (path.empty() || path.is_absolute() ||
            path.has_root_name() || path.has_root_directory())
        {
            return false;
        }
        for (const auto& part : path)
        {
            if (part == "..")
            {
                return false;
            }
        }
        return true;
    }

    bool ParseAbi(const std::string& text, unsigned& abi)
    {
        if (text.empty())
        {
            return false;
        }

        unsigned value = 0;
        for (const char c : text)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }
            value = value * 10 + static_cast<unsigned>(c - '0');
        }
        abi = value;
        return true;
    }

    bool LoadManifest(const fs::path& path, Manifest& manifest, std::string& error)
    {
        std::ifstream input(path);
        if (!input)
        {
            error = "打不开 " + path.string();
            return false;
        }

        std::string line;
        int lineNumber = 0;
        while (std::getline(input, line))
        {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            const std::string_view view = Trim(line);
            if (view.empty() || view.front() == '#')
            {
                continue;
            }

            const std::size_t equals = view.find('=');
            if (equals == std::string_view::npos)
            {
                error = "第 " + std::to_string(lineNumber) + " 行缺少 '='";
                return false;
            }

            const std::string key = ToString(Trim(view.substr(0, equals)));
            const std::string value = ToString(Trim(view.substr(equals + 1)));

            if (key == "name")
            {
                manifest.name = value;
            }
            else if (key == "version")
            {
                manifest.version = value;
            }
            else if (key == "kind")
            {
                manifest.kind = value;
            }
            else if (key == "abi")
            {
                manifest.abi = value;
            }
            else if (key == "file")
            {
                manifest.file = value;
            }
            else if (key == "source")
            {
                manifest.source = value;
            }
        }

        if (manifest.name.empty())
        {
            error = "缺少 name";
            return false;
        }
        if (manifest.version.empty())
        {
            error = "缺少 version";
            return false;
        }
        if (manifest.kind.empty())
        {
            error = "缺少 kind";
            return false;
        }
        if (manifest.abi.empty())
        {
            error = "缺少 abi";
            return false;
        }
        if (manifest.file.empty())
        {
            error = "缺少 file";
            return false;
        }

        if (!IsSafeComponent(manifest.name))
        {
            error = "name 不合法: " + manifest.name;
            return false;
        }
        if (!IsSafeComponent(manifest.version))
        {
            error = "version 不合法: " + manifest.version;
            return false;
        }
        if (manifest.kind != "program" && manifest.kind != "module")
        {
            error = "kind 必须是 program 或 module";
            return false;
        }

        unsigned abi = 0;
        if (!ParseAbi(manifest.abi, abi) || abi != XY_ABI_VERSION)
        {
            error = "abi 必须是 " + std::to_string(XY_ABI_VERSION);
            return false;
        }
        if (!IsSafeRelativePath(manifest.file))
        {
            error = "file 必须是相对路径: " + manifest.file;
            return false;
        }
        if (!manifest.source.empty() && !IsSafeRelativePath(manifest.source))
        {
            error = "source 必须是相对路径: " + manifest.source;
            return false;
        }

        return true;
    }

    bool CollectSources(const fs::path& source,
                        std::vector<fs::path>& sources,
                        std::string& error)
    {
        std::error_code ec;
        if (!fs::is_directory(source, ec) || ec)
        {
            error = "源目录不存在: " + source.string();
            return false;
        }

        const auto options = fs::directory_options::skip_permission_denied;
        fs::recursive_directory_iterator it(source, options, ec);
        const fs::recursive_directory_iterator end;
        if (ec)
        {
            error = "遍历源码失败: " + ec.message();
            return false;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                error = "遍历源码失败: " + ec.message();
                return false;
            }

            std::error_code fileEc;
            if (!it->is_regular_file(fileEc) || fileEc)
            {
                continue;
            }
            if (it->path().extension() == ".cpp")
            {
                sources.push_back(it->path());
            }
        }

        std::sort(sources.begin(), sources.end());
        if (sources.empty())
        {
            error = "源目录里没有 .cpp: " + source.string();
            return false;
        }
        return true;
    }

    fs::path ResolveInclude(const fs::path& source,
                            const char* explicitPath,
                            const fs::path& exeDir)
    {
        if (explicitPath != nullptr && explicitPath[0] != '\0')
        {
            return fs::path(explicitPath);
        }

        if (const char* env = std::getenv("XY_ENGINE_INCLUDE");
            env != nullptr && env[0] != '\0')
        {
            return fs::path(env);
        }

        const fs::path candidates[] = {
            source / "../../Engine/include",
            exeDir / "../../../Engine/include",
            exeDir / "../Engine/include",
            fs::path("Engine/include"),
        };

        for (const fs::path& candidate : candidates)
        {
            std::error_code ec;
            if (fs::exists(candidate / "xy_api.h", ec) && !ec)
            {
                const fs::path canonical = fs::weakly_canonical(candidate, ec);
                return ec ? candidate : canonical;
            }
        }

        return {};
    }

    std::string ResolveCompiler(const char* explicitPath)
    {
        if (explicitPath != nullptr && explicitPath[0] != '\0')
        {
            return explicitPath;
        }
        if (const char* env = std::getenv("CXX"); env != nullptr && env[0] != '\0')
        {
            return env;
        }
        return "g++";
    }

    fs::path ResolvePath(const char* argument, const char* envName, const fs::path& fallback)
    {
        if (argument != nullptr && argument[0] != '\0')
        {
            return fs::path(argument);
        }
        if (const char* env = std::getenv(envName); env != nullptr && env[0] != '\0')
        {
            return fs::path(env);
        }
        return fallback;
    }

    fs::path ExecutableDir(const char* argv0)
    {
        std::error_code ec;
        const fs::path path = fs::absolute(argv0, ec);
        if (ec || path.empty())
        {
            return fs::current_path(ec);
        }
        return path.parent_path();
    }

    fs::path DefaultXyPath(const fs::path& exeDir)
    {
#if defined(_WIN32)
        return exeDir / "X_Y.exe";
#else
        return exeDir / "X_Y";
#endif
    }

    fs::path DefaultPkgPath(const fs::path& exeDir)
    {
#if defined(_WIN32)
        return exeDir / "pkg.dll";
#else
        const fs::path sibling = exeDir / "pkg.so";
        std::error_code ec;
        if (fs::is_regular_file(sibling, ec) && !ec)
        {
            return sibling;
        }
        return exeDir / "../lib/pkg.so";
#endif
    }

    std::vector<std::string> SplitVersion(const std::string& version)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;
        for (;;)
        {
            const std::size_t dot = version.find('.', start);
            if (dot == std::string::npos)
            {
                parts.push_back(version.substr(start));
                break;
            }
            parts.push_back(version.substr(start, dot - start));
            start = dot + 1;
        }
        return parts;
    }

    bool IsDigits(const std::string& text)
    {
        if (text.empty())
        {
            return false;
        }
        for (const char c : text)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }
        }
        return true;
    }

    unsigned long long DigitsValue(const std::string& text)
    {
        unsigned long long value = 0;
        for (const char c : text)
        {
            value = value * 10 + static_cast<unsigned long long>(c - '0');
        }
        return value;
    }

    int CompareVersion(const std::string& a, const std::string& b)
    {
        const std::vector<std::string> left = SplitVersion(a);
        const std::vector<std::string> right = SplitVersion(b);
        const std::size_t count = std::max(left.size(), right.size());

        for (std::size_t i = 0; i < count; ++i)
        {
            const std::string l = i < left.size() ? left[i] : "0";
            const std::string r = i < right.size() ? right[i] : "0";

            if (IsDigits(l) && IsDigits(r))
            {
                const unsigned long long lv = DigitsValue(l);
                const unsigned long long rv = DigitsValue(r);
                if (lv != rv)
                {
                    return lv < rv ? -1 : 1;
                }
            }
            else if (l != r)
            {
                return l < r ? -1 : 1;
            }
        }
        return 0;
    }

    void PrintText(const std::string& text)
    {
        if (!text.empty())
        {
            std::fwrite(text.data(), 1, text.size(), stdout);
            std::fflush(stdout);
        }
    }

    std::string FirstLine(const std::string& text)
    {
        const std::size_t end = text.find_first_of("\r\n");
        return ToString(Trim(std::string_view(text).substr(0, end)));
    }

    void PrintErrors(const std::vector<std::string>& errors)
    {
        for (const auto& error : errors)
        {
            std::fprintf(stderr, "xy: %s\n", error.c_str());
        }
    }

    bool ScanRepo(const fs::path& repo,
                  std::vector<RepoEntry>& entries,
                  std::vector<std::string>& errors)
    {
        std::error_code ec;
        if (!fs::is_directory(repo, ec) || ec)
        {
            errors.push_back("仓库不存在: " + repo.string());
            return false;
        }

        const auto options = fs::directory_options::skip_permission_denied;
        fs::recursive_directory_iterator it(repo, options, ec);
        const fs::recursive_directory_iterator end;
        if (ec)
        {
            errors.push_back("遍历仓库失败: " + ec.message());
            return false;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                errors.push_back("遍历仓库失败: " + ec.message());
                ec.clear();
                continue;
            }

            std::error_code fileEc;
            if (!it->is_regular_file(fileEc) || fileEc)
            {
                continue;
            }
            if (it->path().filename().string() != "package.xy")
            {
                continue;
            }

            Manifest manifest;
            std::string error;
            if (!LoadManifest(it->path(), manifest, error))
            {
                errors.push_back(it->path().string() + ": " + error);
                continue;
            }

            RepoEntry entry;
            entry.directory = it->path().parent_path();
            entry.manifest = manifest;

            std::error_code entryEc;
            entry.binary = fs::is_regular_file(
                entry.directory / manifest.file, entryEc) && !entryEc;
            entryEc.clear();
            entry.source = !manifest.source.empty() &&
                           fs::is_directory(entry.directory / manifest.source, entryEc) &&
                           !entryEc;

            entries.push_back(std::move(entry));
        }

        return true;
    }

    bool SelectEntry(const std::vector<RepoEntry>& entries,
                     const std::string& name,
                     const std::string& version,
                     bool build,
                     RepoEntry& selected,
                     std::string& error)
    {
        const RepoEntry* best = nullptr;

        for (const auto& entry : entries)
        {
            if (entry.manifest.name != name)
            {
                continue;
            }
            if (!version.empty() && entry.manifest.version != version)
            {
                continue;
            }
            if (!entry.binary && !entry.source)
            {
                continue;
            }
            if (build && !entry.source)
            {
                continue;
            }

            if (best == nullptr)
            {
                best = &entry;
                continue;
            }

            const int compared = CompareVersion(entry.manifest.version,
                                                best->manifest.version);
            if (compared > 0)
            {
                best = &entry;
            }
            else if (compared == 0)
            {
                if (build)
                {
                    if (entry.source && !best->source)
                    {
                        best = &entry;
                    }
                }
                else if (entry.binary && !best->binary)
                {
                    best = &entry;
                }
            }
        }

        if (best == nullptr)
        {
            error = "没有找到包 " + name;
            if (!version.empty())
            {
                error += " " + version;
            }
            if (build)
            {
                error += "（--build 需要源码包）";
            }
            return false;
        }

        selected = *best;
        return true;
    }

    void Usage()
    {
        std::printf(
            "xy —— X_Y 最小构建器 / 仓库安装入口\n"
            "\n"
            "用法:\n"
            "  xy build   <源目录> [--out <目录>] [--include <Engine/include>] [--cxx <编译器>]\n"
            "  xy install <name> [version] [--repo <仓库>] [--root <安装根>] [--out <目录>]\n"
            "             [--build] [--force] [--include <Engine/include>] [--cxx <编译器>]\n"
            "             [--x-y <X_Y.exe>] [--pkg <pkg.dll>]\n"
            "  xy doctor  [--repo <仓库>] [--include <Engine/include>] [--cxx <编译器>]\n"
            "             [--x-y <X_Y.exe>] [--pkg <pkg.dll>]\n"
            "  xy help\n"
            "\n"
            "install 的 repo 默认 XY_REPO 或 ./Modules，root 默认 XY_MODULES 或 <xy 目录>/../modules。\n"
            "有二进制直接交给 pkg install；只有源码或 --build 时先 build 再 install。\n");
    }

    int BuildPackage(const fs::path& source,
                     const fs::path& outputRoot,
                     const fs::path& include,
                     const std::string& compiler,
                     bool printNext)
    {
        Manifest manifest;
        std::string error;
        if (!LoadManifest(source / "package.xy", manifest, error))
        {
            std::fprintf(stderr, "xy build: %s\n", error.c_str());
            return 3;
        }

        std::vector<fs::path> sources;
        if (!CollectSources(source, sources, error))
        {
            std::fprintf(stderr, "xy build: %s\n", error.c_str());
            return 3;
        }

        const fs::path destination = outputRoot / manifest.name / manifest.version;
        const fs::path target = destination / manifest.file;

        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        if (ec)
        {
            std::fprintf(stderr, "xy build: 创建输出目录失败: %s\n", ec.message().c_str());
            return 4;
        }

        std::vector<std::string> arguments;
        arguments.push_back("-std=c++20");
        arguments.push_back("-shared");
        arguments.push_back("-O0");
        arguments.push_back("-g");
        arguments.push_back("-finput-charset=UTF-8");
        arguments.push_back("-fexec-charset=UTF-8");
#if !defined(_WIN32)
        arguments.push_back("-fPIC");
#endif
        arguments.push_back("-I");
        arguments.push_back(include.string());
        arguments.push_back("-o");
        arguments.push_back(target.string());
        for (const auto& path : sources)
        {
            arguments.push_back(path.string());
        }
#if defined(_WIN32)
        arguments.push_back("-Wl,--exclude-all-symbols");
#endif

        X_Y::Process::Options options;
        options.Capture = true;
        const X_Y::Process::Result result =
            X_Y::Process::Run(compiler, arguments, options);

        PrintText(result.Output);

        if (!result.Started)
        {
            std::fprintf(stderr, "xy build: 启动编译器失败: %s\n", result.Error.c_str());
            return 5;
        }
        if (!result.Error.empty())
        {
            std::fprintf(stderr, "xy build: 等待编译器失败: %s\n", result.Error.c_str());
            return 5;
        }
        if (result.ExitCode != 0)
        {
            std::fprintf(stderr, "xy build: 编译失败，退出码 %d\n", result.ExitCode);
            return result.ExitCode;
        }

        fs::copy_file(source / "package.xy",
                      destination / "package.xy",
                      fs::copy_options::overwrite_existing,
                      ec);
        if (ec)
        {
            std::fprintf(stderr, "xy build: 复制 package.xy 失败: %s\n", ec.message().c_str());
            return 6;
        }

        std::printf("built %s %s -> %s\n",
                    manifest.name.c_str(),
                    manifest.version.c_str(),
                    target.string().c_str());

        if (printNext)
        {
            std::printf("next: X_Y --module pkg.dll --run pkg install %s --root Modules\n",
                        destination.string().c_str());
        }
        return 0;
    }

    int CmdBuild(int argc, const char* const* argv, const fs::path& exeDir)
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "用法: xy build <源目录> [选项]\n");
            return 2;
        }

        const fs::path source = argv[2];
        fs::path outputRoot = ".cache/out";
        const char* includeOption = nullptr;
        const char* compilerOption = nullptr;

        for (int i = 3; i < argc; ++i)
        {
            const std::string_view option = argv[i];
            if (option == "--out")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy build: --out 缺少值\n");
                    return 2;
                }
                outputRoot = argv[++i];
            }
            else if (option == "--include")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy build: --include 缺少值\n");
                    return 2;
                }
                includeOption = argv[++i];
            }
            else if (option == "--cxx")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy build: --cxx 缺少值\n");
                    return 2;
                }
                compilerOption = argv[++i];
            }
            else
            {
                std::fprintf(stderr, "xy build: 未知参数 '%s'\n", argv[i]);
                return 2;
            }
        }

        const fs::path include = ResolveInclude(source, includeOption, exeDir);
        if (include.empty())
        {
            std::fprintf(stderr,
                         "xy build: 找不到 Engine/include，用 --include 或 XY_ENGINE_INCLUDE 指定\n");
            return 3;
        }

        return BuildPackage(source, outputRoot, include,
                            ResolveCompiler(compilerOption), true);
    }

    int CmdInstall(int argc, const char* const* argv, const fs::path& exeDir)
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "用法: xy install <name> [version] [选项]\n");
            return 2;
        }

        const std::string name = argv[2];
        std::string version;
        int i = 3;
        if (i < argc && std::string_view(argv[i]).rfind("--", 0) != 0)
        {
            version = argv[i++];
        }

        std::string repoArgument;
        std::string rootArgument;
        std::string outArgument;
        std::string includeArgument;
        std::string compilerArgument;
        std::string xyArgument;
        std::string pkgArgument;
        bool build = false;
        bool force = false;

        for (; i < argc; ++i)
        {
            const std::string_view option = argv[i];

            auto takeValue = [&](std::string& target) -> bool
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy install: %s 缺少值\n", argv[i]);
                    return false;
                }
                target = argv[++i];
                return true;
            };

            if (option == "--repo")
            {
                if (!takeValue(repoArgument))
                {
                    return 2;
                }
            }
            else if (option == "--root")
            {
                if (!takeValue(rootArgument))
                {
                    return 2;
                }
            }
            else if (option == "--out")
            {
                if (!takeValue(outArgument))
                {
                    return 2;
                }
            }
            else if (option == "--include")
            {
                if (!takeValue(includeArgument))
                {
                    return 2;
                }
            }
            else if (option == "--cxx")
            {
                if (!takeValue(compilerArgument))
                {
                    return 2;
                }
            }
            else if (option == "--x-y")
            {
                if (!takeValue(xyArgument))
                {
                    return 2;
                }
            }
            else if (option == "--pkg")
            {
                if (!takeValue(pkgArgument))
                {
                    return 2;
                }
            }
            else if (option == "--build")
            {
                build = true;
            }
            else if (option == "--force")
            {
                force = true;
            }
            else
            {
                std::fprintf(stderr, "xy install: 未知参数 '%s'\n", argv[i]);
                return 2;
            }
        }

        const fs::path repo = ResolvePath(
            repoArgument.empty() ? nullptr : repoArgument.c_str(),
            "XY_REPO", "Modules");
        const fs::path root = ResolvePath(
            rootArgument.empty() ? nullptr : rootArgument.c_str(),
            "XY_MODULES", (exeDir / "../modules").lexically_normal());
        const fs::path outputRoot =
            outArgument.empty() ? fs::path(".cache/out") : fs::path(outArgument);

        std::vector<RepoEntry> entries;
        std::vector<std::string> errors;
        if (!ScanRepo(repo, entries, errors))
        {
            PrintErrors(errors);
            return 3;
        }
        PrintErrors(errors);

        RepoEntry selected;
        std::string error;
        if (!SelectEntry(entries, name, version, build, selected, error))
        {
            std::fprintf(stderr, "xy install: %s\n", error.c_str());
            return 3;
        }

        const bool needBuild = build || !selected.binary;
        fs::path packageDirectory = selected.directory;

        if (needBuild)
        {
            if (!selected.source)
            {
                std::fprintf(stderr, "xy install: %s %s 没有源码目录\n",
                             selected.manifest.name.c_str(),
                             selected.manifest.version.c_str());
                return 3;
            }

            const fs::path include = ResolveInclude(
                selected.directory,
                includeArgument.empty() ? nullptr : includeArgument.c_str(),
                exeDir);
            if (include.empty())
            {
                std::fprintf(stderr,
                             "xy install: 找不到 Engine/include，用 --include 或 XY_ENGINE_INCLUDE 指定\n");
                return 3;
            }

            std::printf("building %s %s from %s\n",
                        selected.manifest.name.c_str(),
                        selected.manifest.version.c_str(),
                        selected.directory.string().c_str());

            const int built = BuildPackage(
                selected.directory,
                outputRoot,
                include,
                ResolveCompiler(compilerArgument.empty() ? nullptr : compilerArgument.c_str()),
                false);
            if (built != 0)
            {
                return built;
            }

            packageDirectory = outputRoot / selected.manifest.name / selected.manifest.version;
        }

        const fs::path defaultXy = DefaultXyPath(exeDir);
        const fs::path defaultPkg = DefaultPkgPath(exeDir);

        const fs::path xyPath =
            xyArgument.empty() ? defaultXy : fs::path(xyArgument);
        const fs::path pkgPath =
            pkgArgument.empty() ? defaultPkg : fs::path(pkgArgument);

        std::error_code ec;
        if (!fs::is_regular_file(xyPath, ec) || ec)
        {
            std::fprintf(stderr, "xy install: 找不到 %s（用 --x-y 指定）\n",
                         xyPath.string().c_str());
            return 4;
        }
        if (!fs::is_regular_file(pkgPath, ec) || ec)
        {
            std::fprintf(stderr, "xy install: 找不到 %s（用 --pkg 指定）\n",
                         pkgPath.string().c_str());
            return 4;
        }

        std::vector<std::string> arguments = {
            "--module", pkgPath.string(),
            "--run", "pkg",
            "install", packageDirectory.string(),
            "--root", root.string(),
        };
        if (force)
        {
            arguments.push_back("--force");
        }

        X_Y::Process::Options options;
        options.Capture = true;
        const X_Y::Process::Result result =
            X_Y::Process::Run(xyPath.string(), arguments, options);

        PrintText(result.Output);

        if (!result.Started)
        {
            std::fprintf(stderr, "xy install: 启动 X_Y 失败: %s\n", result.Error.c_str());
            return 5;
        }
        if (!result.Error.empty())
        {
            std::fprintf(stderr, "xy install: 等待 X_Y 失败: %s\n", result.Error.c_str());
            return 5;
        }

        return result.ExitCode;
    }

    int CmdDoctor(int argc, const char* const* argv, const fs::path& exeDir)
    {
        std::string repoArgument;
        std::string includeArgument;
        std::string compilerArgument;
        std::string xyArgument;
        std::string pkgArgument;

        for (int i = 2; i < argc; ++i)
        {
            const std::string_view option = argv[i];
            auto takeValue = [&](std::string& target) -> bool
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy doctor: %s 缺少值\n", argv[i]);
                    return false;
                }
                target = argv[++i];
                return true;
            };

            if (option == "--repo")
            {
                if (!takeValue(repoArgument))
                {
                    return 2;
                }
            }
            else if (option == "--include")
            {
                if (!takeValue(includeArgument))
                {
                    return 2;
                }
            }
            else if (option == "--cxx")
            {
                if (!takeValue(compilerArgument))
                {
                    return 2;
                }
            }
            else if (option == "--x-y")
            {
                if (!takeValue(xyArgument))
                {
                    return 2;
                }
            }
            else if (option == "--pkg")
            {
                if (!takeValue(pkgArgument))
                {
                    return 2;
                }
            }
            else
            {
                std::fprintf(stderr, "xy doctor: 未知参数 '%s'\n", argv[i]);
                return 2;
            }
        }

        const fs::path repo = ResolvePath(
            repoArgument.empty() ? nullptr : repoArgument.c_str(),
            "XY_REPO", "Modules");
        const fs::path include = ResolveInclude(
            ".",
            includeArgument.empty() ? nullptr : includeArgument.c_str(),
            exeDir);
        const std::string compiler = ResolveCompiler(
            compilerArgument.empty() ? nullptr : compilerArgument.c_str());

        int problems = 0;
        X_Y::Process::Options capture;
        capture.Capture = true;

        // 编译器
        const X_Y::Process::Result compilerVersion =
            X_Y::Process::Run(compiler, {"--version"}, capture);
        if (!compilerVersion.Started || !compilerVersion.Error.empty() ||
            compilerVersion.ExitCode != 0)
        {
            std::printf("[FAIL] compiler: %s\n", compiler.c_str());
            if (!compilerVersion.Error.empty())
            {
                std::fprintf(stderr, "       %s\n", compilerVersion.Error.c_str());
            }
            ++problems;
        }
        else
        {
            std::printf("[OK]   compiler: %s\n",
                        FirstLine(compilerVersion.Output).c_str());
        }

        // Engine include
        if (include.empty() ||
            !fs::is_regular_file(include / "xy_api.h") ||
            !fs::is_regular_file(include / "xy_command.h"))
        {
            std::printf("[FAIL] include: 找不到 xy_api.h / xy_command.h\n");
            ++problems;
        }
        else
        {
            std::printf("[OK]   include: %s\n", include.string().c_str());
        }

        // X_Y.exe
        const fs::path xyPath =
            xyArgument.empty() ? DefaultXyPath(exeDir) : fs::path(xyArgument);
        if (!fs::is_regular_file(xyPath))
        {
            std::printf("[WARN] X_Y: %s（xy install 需要）\n",
                        xyPath.string().c_str());
        }
        else
        {
            const X_Y::Process::Result version =
                X_Y::Process::Run(xyPath.string(), {"--version"}, capture);
            if (!version.Started || version.ExitCode != 0)
            {
                std::printf("[WARN] X_Y: %s 跑不起来\n", xyPath.string().c_str());
            }
            else
            {
                std::printf("[OK]   X_Y: %s\n", FirstLine(version.Output).c_str());
            }
        }

        // pkg.dll
        const fs::path pkgPath =
            pkgArgument.empty() ? DefaultPkgPath(exeDir) : fs::path(pkgArgument);
        if (!fs::is_regular_file(pkgPath))
        {
            std::printf("[WARN] pkg: %s（xy install 需要）\n",
                        pkgPath.string().c_str());
        }
        else
        {
            std::printf("[OK]   pkg: %s\n", pkgPath.string().c_str());
        }

        // 仓库
        std::vector<RepoEntry> entries;
        std::vector<std::string> errors;
        const bool repoOk = ScanRepo(repo, entries, errors);
        PrintErrors(errors);

        if (!repoOk)
        {
            std::printf("[FAIL] repo: %s\n", repo.string().c_str());
            ++problems;
        }
        else
        {
            std::size_t binaries = 0;
            std::size_t sources = 0;
            for (const auto& entry : entries)
            {
                if (entry.binary)
                {
                    ++binaries;
                }
                if (entry.source)
                {
                    ++sources;
                }
            }

            std::printf("[OK]   repo: %s（%llu 个包：%llu 二进制，%llu 源码）\n",
                        repo.string().c_str(),
                        static_cast<unsigned long long>(entries.size()),
                        static_cast<unsigned long long>(binaries),
                        static_cast<unsigned long long>(sources));

            if (!errors.empty())
            {
                std::printf("[WARN] repo: %llu 个 manifest 无效\n",
                            static_cast<unsigned long long>(errors.size()));
                problems += static_cast<int>(errors.size());
            }
        }

        if (problems == 0)
        {
            std::printf("doctor: OK\n");
        }
        else
        {
            std::printf("doctor: %d 个问题\n", problems);
        }
        return problems == 0 ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    const fs::path exeDir = ExecutableDir(argv[0]);

    if (argc < 2)
    {
        Usage();
        return 2;
    }

    const std::string_view command = argv[1];
    if (command == "build")
    {
        return CmdBuild(argc, argv, exeDir);
    }
    if (command == "install")
    {
        return CmdInstall(argc, argv, exeDir);
    }
    if (command == "doctor")
    {
        return CmdDoctor(argc, argv, exeDir);
    }
    if (command == "help" || command == "--help" || command == "-h")
    {
        Usage();
        return 0;
    }

    std::fprintf(stderr, "xy: 未知命令 '%s'\n", argv[1]);
    Usage();
    return 2;
}
