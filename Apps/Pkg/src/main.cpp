// pkg —— X_Y 的只读包管理命令 DLL。
//
// v1 只做 list / info / verify：
//   - 递归扫描 root 下的 package.xy
//   - 不绑定 Modules 的目录层级，布局以后改，pkg 不用改
//   - root 优先级：命令行参数 > 环境变量 XY_MODULES > ./Modules
//
// 不做 install / remove / 下载 / 依赖求解 / 自己 LoadLibrary。

#include <xy_api.h>
#include <xy_command.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    struct Manifest
    {
        fs::path path;
        std::map<std::string, std::string> fields;

        std::string Get(const std::string& key) const
        {
            const auto it = fields.find(key);
            return it == fields.end() ? std::string{} : it->second;
        }
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

    std::string PathText(const fs::path& path)
    {
        return path.string();
    }

    bool LoadManifest(const fs::path& path, Manifest& manifest, std::string& error)
    {
        std::ifstream input(path);
        if (!input)
        {
            error = "打不开文件";
            return false;
        }

        manifest.path = path;
        manifest.fields.clear();

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

            const std::string_view keyView = Trim(view.substr(0, equals));
            const std::string_view valueView = Trim(view.substr(equals + 1));
            if (keyView.empty())
            {
                error = "第 " + std::to_string(lineNumber) + " 行 key 为空";
                return false;
            }

            manifest.fields[std::string(keyView)] = std::string(valueView);
        }

        const char* required[] = {"name", "version", "kind", "abi", "file"};
        for (const char* key : required)
        {
            if (manifest.Get(key).empty())
            {
                error = std::string("缺少字段 ") + key;
                return false;
            }
        }

        return true;
    }

    void PrintErrors(const std::vector<std::string>& errors)
    {
        for (const auto& error : errors)
        {
            std::fprintf(stderr, "pkg: %s\n", error.c_str());
        }
    }

    bool Scan(const fs::path& root,
              std::vector<Manifest>& manifests,
              std::vector<std::string>& errors)
    {
        std::error_code ec;
        if (!fs::exists(root, ec) || ec)
        {
            errors.push_back("根目录不存在: " + PathText(root));
            return false;
        }
        if (!fs::is_directory(root, ec) || ec)
        {
            errors.push_back("不是目录: " + PathText(root));
            return false;
        }

        const auto options = fs::directory_options::skip_permission_denied;
        fs::recursive_directory_iterator it(root, options, ec);
        const fs::recursive_directory_iterator end;
        if (ec)
        {
            errors.push_back("遍历失败: " + ec.message());
            return false;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                errors.push_back("遍历失败: " + ec.message());
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
            if (LoadManifest(it->path(), manifest, error))
            {
                manifests.push_back(std::move(manifest));
            }
            else
            {
                errors.push_back(PathText(it->path()) + ": " + error);
            }
        }

        std::sort(manifests.begin(), manifests.end(),
                  [](const Manifest& a, const Manifest& b)
                  {
                      if (a.Get("name") != b.Get("name"))
                      {
                          return a.Get("name") < b.Get("name");
                      }
                      if (a.Get("version") != b.Get("version"))
                      {
                          return a.Get("version") < b.Get("version");
                      }
                      return a.path < b.path;
                  });

        return true;
    }

    fs::path ResolveRoot(const char* argument)
    {
        if (argument != nullptr && argument[0] != '\0')
        {
            return fs::path(argument);
        }

        if (const char* env = std::getenv("XY_MODULES"); env != nullptr && env[0] != '\0')
        {
            return fs::path(env);
        }

        return fs::path("Modules");
    }

    bool ParseRepoArguments(int argc,
                            const char* const* argv,
                            int start,
                            fs::path& root,
                            std::string& error)
    {
        bool positionalUsed = false;

        for (int i = start; i < argc; ++i)
        {
            const std::string_view argument = argv[i];
            if (argument == "--repo")
            {
                if (i + 1 >= argc)
                {
                    error = "--repo 缺少值";
                    return false;
                }
                root = fs::path(argv[++i]);
            }
            else if (argument.rfind("--", 0) == 0)
            {
                error = "未知参数 '" + std::string(argument) + "'";
                return false;
            }
            else if (!positionalUsed)
            {
                root = fs::path(argument);
                positionalUsed = true;
            }
            else
            {
                error = "多余的参数: " + std::string(argument);
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

    bool ValidateForInstall(const Manifest& manifest,
                            const fs::path& source,
                            std::string& error)
    {
        const std::string name = manifest.Get("name");
        const std::string version = manifest.Get("version");

        if (!IsSafeComponent(name))
        {
            error = "包名不合法: " + name;
            return false;
        }
        if (!IsSafeComponent(version))
        {
            error = "版本号不合法: " + version;
            return false;
        }
        if (manifest.Get("kind") != "program" &&
            manifest.Get("kind") != "module")
        {
            error = "kind 必须是 program 或 module";
            return false;
        }

        unsigned abi = 0;
        if (!ParseAbi(manifest.Get("abi"), abi))
        {
            error = "abi 不是数字";
            return false;
        }
        if (abi != XY_ABI_VERSION)
        {
            error = "abi 不匹配：包是 " + manifest.Get("abi") +
                    "，内核是 " + std::to_string(XY_ABI_VERSION);
            return false;
        }

        const std::string file = manifest.Get("file");
        if (!IsSafeRelativePath(file))
        {
            error = "file 必须是包内相对路径: " + file;
            return false;
        }

        std::error_code ec;
        if (!fs::is_regular_file(source / file, ec) || ec)
        {
            error = "文件不存在: " + PathText(source / file);
            return false;
        }

        return true;
    }

    void Usage()
    {
        std::printf(
            "pkg —— 包管理\n"
            "\n"
            "用法:\n"
            "  pkg list   [root] [--repo <dir>]\n"
            "  pkg info   <name> [root] [--repo <dir>]\n"
            "  pkg verify [root] [--repo <dir>]\n"
            "  pkg install <源目录> [--root <dir>] [--force]\n"
            "  pkg remove  <name> [version] [--root <dir>] [--all]\n"
            "\n"
            "root / --repo 指定仓库，默认 XY_MODULES（X_Y 启动时设为 bin/../modules），\n"
            "没有就用 ./Modules\n"
            "安装布局: <root>/<name>/<version>/package.xy + 包内文件\n");
    }

    void PrintManifest(const Manifest& manifest)
    {
        const char* order[] = {
            "name", "version", "kind", "abi", "entry", "file", "deps", "source"
        };

        for (const char* key : order)
        {
            const std::string value = manifest.Get(key);
            if (!value.empty())
            {
                std::printf("  %-8s = %s\n", key, value.c_str());
            }
        }

        for (const auto& [key, value] : manifest.fields)
        {
            const bool known =
                std::find(std::begin(order), std::end(order), key) != std::end(order);
            if (!known && !value.empty())
            {
                std::printf("  %-8s = %s\n", key.c_str(), value.c_str());
            }
        }

        std::printf("  %-8s = %s\n", "path", PathText(manifest.path).c_str());
    }

    int CmdList(int argc, const char* const* argv)
    {
        fs::path root = ResolveRoot(nullptr);
        std::string argumentError;
        if (!ParseRepoArguments(argc, argv, 2, root, argumentError))
        {
            std::fprintf(stderr, "pkg list: %s\n", argumentError.c_str());
            return 2;
        }

        std::vector<Manifest> manifests;
        std::vector<std::string> errors;
        Scan(root, manifests, errors);
        PrintErrors(errors);

        if (manifests.empty())
        {
            std::printf("没有找到 package.xy\n");
            return errors.empty() ? 0 : 1;
        }

        std::printf("%-16s %-10s %-8s %-4s %s\n",
                    "NAME", "VERSION", "KIND", "ABI", "PATH");
        for (const auto& manifest : manifests)
        {
            std::printf("%-16s %-10s %-8s %-4s %s\n",
                        manifest.Get("name").c_str(),
                        manifest.Get("version").c_str(),
                        manifest.Get("kind").c_str(),
                        manifest.Get("abi").c_str(),
                        PathText(manifest.path).c_str());
        }

        return errors.empty() ? 0 : 1;
    }

    int CmdInfo(int argc, const char* const* argv)
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "用法: pkg info <name> [root] [--repo <dir>]\n");
            return 2;
        }

        const std::string name = argv[2];
        fs::path root = ResolveRoot(nullptr);
        std::string argumentError;
        if (!ParseRepoArguments(argc, argv, 3, root, argumentError))
        {
            std::fprintf(stderr, "pkg info: %s\n", argumentError.c_str());
            return 2;
        }

        std::vector<Manifest> manifests;
        std::vector<std::string> errors;
        Scan(root, manifests, errors);
        PrintErrors(errors);

        bool found = false;
        for (const auto& manifest : manifests)
        {
            if (manifest.Get("name") == name)
            {
                PrintManifest(manifest);
                found = true;
            }
        }

        if (!found)
        {
            std::fprintf(stderr, "没有包: %s\n", name.c_str());
            return 3;
        }

        return errors.empty() ? 0 : 1;
    }

    int CmdVerify(int argc, const char* const* argv)
    {
        fs::path root = ResolveRoot(nullptr);
        std::string argumentError;
        if (!ParseRepoArguments(argc, argv, 2, root, argumentError))
        {
            std::fprintf(stderr, "pkg verify: %s\n", argumentError.c_str());
            return 2;
        }

        std::vector<Manifest> manifests;
        std::vector<std::string> errors;
        Scan(root, manifests, errors);
        PrintErrors(errors);

        int failures = static_cast<int>(errors.size());
        std::set<std::string> names;

        for (const auto& manifest : manifests)
        {
            std::string reason;
            unsigned abi = 0;

            if (!ParseAbi(manifest.Get("abi"), abi))
            {
                reason = "abi 不是数字";
            }
            else if (abi != XY_ABI_VERSION)
            {
                reason = "abi 不匹配：包是 " + manifest.Get("abi") +
                         "，内核是 " + std::to_string(XY_ABI_VERSION);
            }
            else if (manifest.Get("kind") != "program" &&
                     manifest.Get("kind") != "module")
            {
                reason = "kind 必须是 program 或 module";
            }
            else
            {
                const fs::path file =
                    manifest.path.parent_path() / manifest.Get("file");
                std::error_code ec;
                if (!fs::exists(file, ec) || ec)
                {
                    reason = "文件不存在: " + PathText(file);
                }
            }

            const std::string identity =
                manifest.Get("name") + "@" + manifest.Get("version");
            if (reason.empty() && !names.insert(identity).second)
            {
                reason = "包名+版本重复: " + manifest.Get("name") +
                         " " + manifest.Get("version");
            }

            if (reason.empty())
            {
                std::printf("[OK]   %s %s\n",
                            manifest.Get("name").c_str(),
                            manifest.Get("version").c_str());
            }
            else
            {
                std::printf("[FAIL] %s: %s\n",
                            PathText(manifest.path).c_str(),
                            reason.c_str());
                ++failures;
            }
        }

        if (manifests.empty())
        {
            std::printf("没有找到 package.xy\n");
        }

        return failures == 0 ? 0 : 1;
    }

    int CmdInstall(int argc, const char* const* argv)
    {
        if (argc < 3)
        {
            std::fprintf(stderr,
                         "用法: pkg install <源目录> [--root <dir>] [--force]\n");
            return 2;
        }

        const fs::path source = argv[2];
        fs::path root = ResolveRoot(nullptr);
        bool force = false;

        for (int i = 3; i < argc; ++i)
        {
            const std::string_view option = argv[i];
            if (option == "--root")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "pkg install: --root 缺少值\n");
                    return 2;
                }
                root = fs::path(argv[++i]);
            }
            else if (option == "--force")
            {
                force = true;
            }
            else
            {
                std::fprintf(stderr, "pkg install: 未知参数 '%s'\n", argv[i]);
                return 2;
            }
        }

        std::error_code ec;
        if (!fs::is_directory(source, ec) || ec)
        {
            std::fprintf(stderr, "pkg install: 源目录不存在: %s\n",
                         PathText(source).c_str());
            return 3;
        }

        Manifest manifest;
        std::string error;
        if (!LoadManifest(source / "package.xy", manifest, error))
        {
            std::fprintf(stderr, "pkg install: %s\n", error.c_str());
            return 3;
        }
        if (!ValidateForInstall(manifest, source, error))
        {
            std::fprintf(stderr, "pkg install: %s\n", error.c_str());
            return 3;
        }

        const fs::path destination =
            root / manifest.Get("name") / manifest.Get("version");
        const fs::path staging(destination.string() + ".tmp");

        ec.clear();
        const bool destinationExists = fs::exists(destination, ec);
        if (ec)
        {
            std::fprintf(stderr, "pkg install: 检查目标失败: %s\n", ec.message().c_str());
            return 4;
        }
        if (destinationExists && !force)
        {
            std::fprintf(stderr,
                         "pkg install: 目标已存在: %s（用 --force 覆盖）\n",
                         PathText(destination).c_str());
            return 4;
        }

        fs::remove_all(staging, ec);
        ec.clear();
        fs::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            std::fprintf(stderr, "pkg install: 创建目录失败: %s\n", ec.message().c_str());
            return 5;
        }

        fs::copy(source, staging, fs::copy_options::recursive, ec);
        if (ec)
        {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
            std::fprintf(stderr, "pkg install: 复制失败: %s\n", ec.message().c_str());
            return 5;
        }

        if (destinationExists)
        {
            fs::remove_all(destination, ec);
            if (ec)
            {
                std::error_code ignored;
                fs::remove_all(staging, ignored);
                std::fprintf(stderr, "pkg install: 删除旧版本失败: %s\n",
                             ec.message().c_str());
                return 5;
            }
        }

        fs::rename(staging, destination, ec);
        if (ec)
        {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
            std::fprintf(stderr, "pkg install: 落地失败: %s\n", ec.message().c_str());
            return 5;
        }

        std::printf("installed %s %s -> %s\n",
                    manifest.Get("name").c_str(),
                    manifest.Get("version").c_str(),
                    PathText(destination).c_str());
        return 0;
    }

    int CmdRemove(int argc, const char* const* argv)
    {
        if (argc < 3)
        {
            std::fprintf(stderr,
                         "用法: pkg remove <name> [version] [--root <dir>] [--all]\n");
            return 2;
        }

        const std::string name = argv[2];
        std::string version;
        bool all = false;
        fs::path root = ResolveRoot(nullptr);

        int i = 3;
        if (i < argc && std::string_view(argv[i]).rfind("--", 0) != 0)
        {
            version = argv[i++];
        }

        for (; i < argc; ++i)
        {
            const std::string_view option = argv[i];
            if (option == "--root")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "pkg remove: --root 缺少值\n");
                    return 2;
                }
                root = fs::path(argv[++i]);
            }
            else if (option == "--all")
            {
                all = true;
            }
            else
            {
                std::fprintf(stderr, "pkg remove: 未知参数 '%s'\n", argv[i]);
                return 2;
            }
        }

        if (!IsSafeComponent(name))
        {
            std::fprintf(stderr, "pkg remove: 包名不合法: %s\n", name.c_str());
            return 2;
        }
        if (!version.empty() && !IsSafeComponent(version))
        {
            std::fprintf(stderr, "pkg remove: 版本号不合法: %s\n", version.c_str());
            return 2;
        }
        if (all && !version.empty())
        {
            std::fprintf(stderr, "pkg remove: 版本号和 --all 不能同时给\n");
            return 2;
        }
        if (!all && version.empty())
        {
            std::fprintf(stderr, "pkg remove: 请指定版本，或用 --all\n");
            return 2;
        }

        std::error_code ec;
        const fs::path nameDirectory = root / name;

        if (all)
        {
            if (!fs::exists(nameDirectory, ec) || ec)
            {
                std::fprintf(stderr, "pkg remove: 没有这个包: %s\n", name.c_str());
                return 3;
            }

            fs::remove_all(nameDirectory, ec);
            if (ec)
            {
                std::fprintf(stderr, "pkg remove: 删除失败: %s\n", ec.message().c_str());
                return 4;
            }

            std::printf("removed %s (all versions)\n", name.c_str());
            return 0;
        }

        const fs::path target = nameDirectory / version;
        if (!fs::exists(target, ec) || ec)
        {
            std::fprintf(stderr, "pkg remove: 没有这个版本: %s %s\n",
                         name.c_str(), version.c_str());
            return 3;
        }

        fs::remove_all(target, ec);
        if (ec)
        {
            std::fprintf(stderr, "pkg remove: 删除失败: %s\n", ec.message().c_str());
            return 4;
        }

        // 版本目录删完后，父目录空了也顺手收掉。
        ec.clear();
        if (fs::exists(nameDirectory, ec) && fs::is_empty(nameDirectory, ec) && !ec)
        {
            fs::remove(nameDirectory, ec);
        }

        std::printf("removed %s %s\n", name.c_str(), version.c_str());
        return 0;
    }

    int CmdPkg(void* self, int argc, const char* const* argv)
    {
        (void)self;

        if (argc < 2)
        {
            Usage();
            return 2;
        }

        const std::string_view subcommand = argv[1];
        if (subcommand == "list")
        {
            return CmdList(argc, argv);
        }
        if (subcommand == "info")
        {
            return CmdInfo(argc, argv);
        }
        if (subcommand == "verify")
        {
            return CmdVerify(argc, argv);
        }
        if (subcommand == "install")
        {
            return CmdInstall(argc, argv);
        }
        if (subcommand == "remove")
        {
            return CmdRemove(argc, argv);
        }
        if (subcommand == "help")
        {
            Usage();
            return 0;
        }

        std::fprintf(stderr, "pkg: 未知子命令 '%s'\n", argv[1]);
        Usage();
        return 2;
    }

    const XyCommandV1 kPkg = {
        sizeof(XyCommandV1),
        "pkg",
        "包管理：pkg list/info/verify/install/remove",
        &CmdPkg,
        nullptr
    };

    const XyCommandV1* const kCommands[] = {&kPkg, nullptr};
}

XY_EXPORT uint32_t xy_abi_version_v1(void)
{
    return XY_ABI_VERSION;
}

XY_EXPORT const XyCommandV1* const* xy_commands_v1(const XyHostV1* host)
{
    if (host == nullptr || host->abi_version != XY_ABI_VERSION)
    {
        return nullptr;
    }
    return kCommands;
}
