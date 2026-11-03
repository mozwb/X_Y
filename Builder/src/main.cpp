// xy builds packages, prepares development modules, and installs runnable packages.

#include "Platform/Process.h"

#include <xy_api.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
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
        std::string deps;
        std::string libs;
        std::string libsWindows;
        std::string libsLinux;
        std::string includes;
        std::string libDirs;
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
            else if (key == "deps")
            {
                manifest.deps = value;
            }
            else if (key == "libs")
            {
                manifest.libs = value;
            }
            else if (key == "libs_windows")
            {
                manifest.libsWindows = value;
            }
            else if (key == "libs_linux")
            {
                manifest.libsLinux = value;
            }
            else if (key == "includes")
            {
                manifest.includes = value;
            }
            else if (key == "libDirs")
            {
                manifest.libDirs = value;
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
        if (manifest.kind != "program" &&
            manifest.kind != "module" &&
            manifest.kind != "executable" &&
            manifest.kind != "library")
        {
            error = "kind 必须是 program、module、executable 或 library";
            return false;
        }

        unsigned abi = 0;
        if (!ParseAbi(manifest.abi, abi) || abi != XY_ABI_VERSION)
        {
            error = "abi 必须是 " + std::to_string(XY_ABI_VERSION);
            return false;
        }
        if (!manifest.file.empty() && !IsSafeRelativePath(manifest.file))
        {
            error = "file 必须是相对路径: " + manifest.file;
            return false;
        }
        if (manifest.file.empty() && manifest.kind != "library")
        {
            error = "缺少 file";
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

    struct BuildEnv
    {
        fs::path config;
        fs::path root;
        fs::path bin;
        fs::path modules;
        fs::path programs;
        fs::path lib;
        fs::path temp;
        fs::path source;
    };

    std::string TrimCopy(std::string text);

    std::unordered_map<std::string, std::string> ReadFlatJson(const fs::path& path)
    {
        std::unordered_map<std::string, std::string> out;
        std::ifstream in(path);
        if (!in)
        {
            return out;
        }

        std::string line;
        while (std::getline(in, line))
        {
            const std::string trimmed = TrimCopy(line);
            if (trimmed.empty() || trimmed == "{" || trimmed == "}")
            {
                continue;
            }

            const std::size_t colon = trimmed.find(':');
            const std::size_t q1 = trimmed.find('"');
            if (colon == std::string::npos || q1 == std::string::npos || q1 > colon)
            {
                continue;
            }
            const std::size_t q2 = trimmed.find('"', q1 + 1);
            if (q2 == std::string::npos || q2 > colon)
            {
                continue;
            }

            const std::size_t q3 = trimmed.find('"', colon + 1);
            if (q3 == std::string::npos)
            {
                continue;
            }
            const std::size_t q4 = trimmed.find('"', q3 + 1);
            if (q4 == std::string::npos)
            {
                continue;
            }

            out[trimmed.substr(q1 + 1, q2 - q1 - 1)] =
                trimmed.substr(q3 + 1, q4 - q3 - 1);
        }
        return out;
    }

    fs::path ResolveUnder(const fs::path& base, const std::string& value,
                          const char* fallback)
    {
        fs::path path = value.empty() ? fs::path(fallback) : fs::path(value);
        if (path.is_absolute())
        {
            return path.lexically_normal();
        }
        return (base / path).lexically_normal();
    }

    fs::path FindDefaultSource(const fs::path& exeDir)
    {
        const fs::path candidates[] = {
            exeDir / "../../../../Modules/Modules",
            fs::current_path() / "Modules/Modules",
            fs::current_path().parent_path() / "Modules/Modules"
        };
        for (const auto& candidate : candidates)
        {
            std::error_code ec;
            if (fs::is_directory(candidate, ec) && !ec)
            {
                return candidate.lexically_normal();
            }
        }
        return (fs::current_path() / "Modules/Modules").lexically_normal();
    }

    BuildEnv LoadBuildEnv(const fs::path& exeDir)
    {
        BuildEnv env;
        env.root = (exeDir / "..").lexically_normal();
        env.bin = env.root / "bin";
        env.modules = env.root / "modules";
        env.programs = env.root / "programs";
        env.lib = env.root / "lib";
        env.temp = env.root / ".cache/tmp";
        env.source = FindDefaultSource(exeDir);

        const fs::path candidates[] = {
            fs::current_path() / "enve.json",
            fs::current_path() / "out/enve.json",
            exeDir / "enve.json",
            exeDir / "../enve.json",
            exeDir / "../../../enve.json",
            exeDir / "../../../../enve.json"
        };
        for (const auto& candidate : candidates)
        {
            std::error_code ec;
            if (fs::is_regular_file(candidate, ec) && !ec)
            {
                env.config = candidate.lexically_normal();
                break;
            }
        }
        if (env.config.empty())
        {
            return env;
        }

        const auto kv = ReadFlatJson(env.config);
        auto get = [&](std::initializer_list<const char*> keys) -> std::string
        {
            for (const char* key : keys)
            {
                const auto it = kv.find(key);
                if (it != kv.end())
                {
                    return it->second;
                }
            }
            return {};
        };

        const fs::path configDir = env.config.parent_path();
        const bool configInOutputDir = configDir.filename() == "out";
        const fs::path base = configInOutputDir
                                  ? configDir.parent_path()
                                  : configDir;
        const std::string rootValue = get({"root"});
        env.root = rootValue.empty()
                       ? configDir
                       : ResolveUnder(base, rootValue, ".");
        env.bin = ResolveUnder(env.root, get({"BIN", "bin"}), "bin");
        env.modules = ResolveUnder(env.root, get({"package", "modules"}), "modules");
        env.programs = ResolveUnder(env.root, get({"program", "programs"}), "programs");
        env.lib = ResolveUnder(env.root, get({"lib"}), "lib");
        env.temp = ResolveUnder(env.root, get({"temp"}), ".cache/tmp");

        const std::string sourceValue = get({"Modules", "source"});
        if (!sourceValue.empty())
        {
            env.source = ResolveUnder(base, sourceValue, "Modules/Modules");
        }
        return env;
    }

    void SetBuildTempDir(const BuildEnv& env)
    {
        std::error_code ec;
        fs::create_directories(env.temp, ec);
        if (ec)
        {
            return;
        }

        const std::string text = env.temp.string();
#if defined(_WIN32)
        ::_putenv_s("TMP", text.c_str());
        ::_putenv_s("TEMP", text.c_str());
#else
        setenv("TMPDIR", text.c_str(), 1);
#endif
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
        bool libraryFound = false;

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
            if (entry.manifest.kind == "library")
            {
                libraryFound = true;
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
            if (libraryFound)
            {
                error = name + " 是开发库，请使用 xy module " + name;
                return false;
            }
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

    struct ModuleManifest
    {
        std::string name;
        std::string version = "0.0.1";
        std::string kind = "library";
        std::string sources = "src";
        std::string include = ".";
        std::string headers = ".";
        std::string exclude;
        std::string file;
        std::string staticFile;
        std::string sharedFile;
        std::string importLib;
        std::string deps;
        std::string libs;
        std::string libsWindows;
        std::string libsLinux;
        std::string libDirs;
        std::string defines;
    };

    std::string TrimCopy(std::string text)
    {
        const std::size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return {};
        }
        const std::size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    std::vector<std::string> SplitList(const std::string& text)
    {
        std::vector<std::string> out;
        std::string current;
        for (const char c : text)
        {
            if (c == ',' || c == ';')
            {
                const std::string item = TrimCopy(current);
                if (!item.empty())
                {
                    out.push_back(item);
                }
                current.clear();
            }
            else
            {
                current.push_back(c);
            }
        }
        const std::string item = TrimCopy(current);
        if (!item.empty())
        {
            out.push_back(item);
        }
        return out;
    }

    struct DependencySpec
    {
        std::string name;
        std::string link = "auto";
    };

    DependencySpec ParseDependency(const std::string& token)
    {
        DependencySpec spec;
        const std::size_t colon = token.rfind(':');
        if (colon != std::string::npos)
        {
            const std::string mode = token.substr(colon + 1);
            if (mode == "static" || mode == "shared" || mode == "auto")
            {
                spec.name = token.substr(0, colon);
                spec.link = mode;
                return spec;
            }
        }
        spec.name = token;
        return spec;
    }

    bool LoadModuleManifest(const fs::path& moduleDir, ModuleManifest& out,
                            std::string& error)
    {
        std::error_code ec;
        fs::path path = moduleDir / "package.xy";
        if (!fs::is_regular_file(path, ec) || ec)
        {
            path = moduleDir / "module.xy";
        }

        std::ifstream in(path);
        if (!in)
        {
            error = "读不到 " + path.string();
            return false;
        }

        std::string line;
        while (std::getline(in, line))
        {
            const std::string trimmed = TrimCopy(line);
            if (trimmed.empty() || trimmed[0] == '#')
            {
                continue;
            }

            const std::size_t eq = trimmed.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            const std::string key = TrimCopy(trimmed.substr(0, eq));
            const std::string value = TrimCopy(trimmed.substr(eq + 1));

            if (key == "name") out.name = value;
            else if (key == "version") out.version = value;
            else if (key == "kind") out.kind = value;
            else if (key == "sources" || key == "source") out.sources = value;
            else if (key == "include") out.include = value;
            else if (key == "headers") out.headers = value;
            else if (key == "exclude") out.exclude = value;
            else if (key == "file") out.file = value;
            else if (key == "static_file") out.staticFile = value;
            else if (key == "shared_file") out.sharedFile = value;
            else if (key == "import_lib") out.importLib = value;
            else if (key == "deps") out.deps = value;
            else if (key == "libs") out.libs = value;
            else if (key == "libs_windows") out.libsWindows = value;
            else if (key == "libs_linux") out.libsLinux = value;
            else if (key == "libDirs" || key == "lib_dirs") out.libDirs = value;
            else if (key == "defines") out.defines = value;
        }

        if (out.name.empty())
        {
            error = path.string() + " 缺少 name";
            return false;
        }
        if (out.kind != "library")
        {
            error = path.string() + " kind 必须是 library";
            return false;
        }
        if (!IsSafeComponent(out.name) || !IsSafeComponent(out.version))
        {
            error = path.string() + " name 或 version 不合法";
            return false;
        }
        return true;
    }

    struct ModuleBuildContext
    {
        fs::path modulesRoot;
        fs::path outRoot;
        fs::path libRoot;
        fs::path tempRoot;
        fs::path engineInclude;
        std::string compiler;
        std::set<std::string> built;
        std::vector<std::string> stack;
    };

    struct PreparedModule
    {
        std::string link;
        fs::path library;
    };

    bool FindModuleSource(const fs::path& modulesRoot,
                         const std::string& name,
                         const std::string& requestedVersion,
                         fs::path& moduleDir,
                         ModuleManifest& manifest,
                         std::string& error)
    {
        const fs::path direct = modulesRoot / name;
        std::vector<fs::path> candidates;
        std::error_code ec;

        if (fs::is_regular_file(direct / "package.xy", ec) ||
            fs::is_regular_file(direct / "module.xy", ec))
        {
            candidates.push_back(direct);
        }
        ec.clear();
        if (fs::is_directory(direct, ec) && !ec)
        {
            for (fs::directory_iterator it(direct, ec), end; it != end && !ec;
                 it.increment(ec))
            {
                if (!it->is_directory(ec) || ec)
                {
                    continue;
                }
                const fs::path child = it->path();
                if (fs::is_regular_file(child / "package.xy", ec) ||
                    fs::is_regular_file(child / "module.xy", ec))
                {
                    candidates.push_back(child);
                }
            }
        }

        const auto options = fs::directory_options::skip_permission_denied;
        fs::recursive_directory_iterator it(modulesRoot, options, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            const std::string filename = it->path().filename().string();
            if (filename != "package.xy" && filename != "module.xy")
            {
                continue;
            }
            candidates.push_back(it->path().parent_path());
        }

        bool found = false;
        for (const fs::path& candidate : candidates)
        {
            ModuleManifest current;
            std::string loadError;
            if (!LoadModuleManifest(candidate, current, loadError) ||
                current.name != name ||
                (!requestedVersion.empty() &&
                 current.version != requestedVersion))
            {
                continue;
            }
            if (!found || CompareVersion(current.version, manifest.version) > 0)
            {
                found = true;
                moduleDir = candidate;
                manifest = std::move(current);
            }
        }

        if (!found)
        {
            error = "模块仓库中没有找到 " + name;
            if (!requestedVersion.empty())
            {
                error += " " + requestedVersion;
            }
            error += ": " + modulesRoot.string();
            return false;
        }
        return true;
    }

    bool CopyModuleHeaders(const fs::path& moduleDir,
                           const ModuleManifest& manifest,
                           const fs::path& destination,
                           std::string& error)
    {
        const fs::path base = moduleDir / manifest.headers;
        std::error_code ec;
        if (!fs::is_directory(base, ec))
        {
            if (ec)
            {
                error = "读取模块头文件目录失败: " + base.string() +
                        ": " + ec.message();
                return false;
            }
            return true;
        }

        fs::recursive_directory_iterator it(base, ec);
        const fs::recursive_directory_iterator end;
        if (ec)
        {
            error = "遍历模块头文件失败: " + base.string() +
                    ": " + ec.message();
            return false;
        }
        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                error = "遍历模块头文件失败: " + base.string() +
                        ": " + ec.message();
                return false;
            }
            if (!it->is_regular_file(ec))
            {
                if (ec)
                {
                    error = "读取模块头文件状态失败: " + it->path().string() +
                            ": " + ec.message();
                    return false;
                }
                continue;
            }

            const std::string ext = it->path().extension().string();
            if (ext != ".h" && ext != ".hpp" && ext != ".inl")
            {
                continue;
            }

            const fs::path relative = it->path().lexically_relative(base);
            if (relative.empty() ||
                relative.generic_string().rfind("src/", 0) == 0)
            {
                continue;
            }

            const fs::path target = destination / relative;
            fs::create_directories(target.parent_path(), ec);
            if (ec)
            {
                error = "创建头文件目录失败: " + target.parent_path().string();
                return false;
            }
            std::error_code removeEc;
            fs::remove(target, removeEc);
            if (removeEc)
            {
                error = "替换头文件失败: " + target.string() +
                        ": " + removeEc.message();
                return false;
            }
            fs::copy_file(it->path(), target, fs::copy_options::none, ec);
            if (ec)
            {
                error = "复制头文件失败: " + it->path().string() + " -> " +
                        target.string() + ": " + ec.message();
                return false;
            }
        }
        if (ec)
        {
            error = "遍历模块头文件失败: " + base.string() +
                    ": " + ec.message();
            return false;
        }
        return true;
    }

    bool CopyDirectory(const fs::path& source, const fs::path& destination,
                       std::string& error)
    {
        std::error_code ec;
        if (!fs::is_directory(source, ec) || ec)
        {
            error = "源码目录不存在: " + source.string();
            return false;
        }
        fs::create_directories(destination, ec);
        if (ec)
        {
            error = "创建目录失败: " + destination.string() + ": " + ec.message();
            return false;
        }

        for (fs::recursive_directory_iterator it(source, ec), end;
             it != end && !ec; it.increment(ec))
        {
            const fs::path relative = it->path().lexically_relative(source);
            const fs::path target = destination / relative;
            if (it->is_directory(ec))
            {
                fs::create_directories(target, ec);
            }
            else if (!ec && it->is_regular_file(ec))
            {
                fs::create_directories(target.parent_path(), ec);
                if (!ec)
                {
                    std::error_code removeError;
                    fs::remove(target, removeError);
                    if (removeError)
                    {
                        error = "替换模块文件失败: " + target.string() +
                                ": " + removeError.message();
                        return false;
                    }
                    fs::copy_file(it->path(), target,
                                  fs::copy_options::none, ec);
                }
            }
            if (ec)
            {
                error = "复制模块文件失败: " + it->path().string() +
                        " -> " + target.string() + ": " + ec.message();
                return false;
            }
        }
        if (ec)
        {
            error = "遍历源码目录失败: " + ec.message();
            return false;
        }
        return true;
    }

    fs::path FindModuleLibrary(const fs::path& moduleDir,
                               const ModuleManifest& manifest,
                               const std::string& name,
                               const std::string& link,
                               bool importLibrary)
    {
        std::vector<fs::path> candidates;
        if (link == "static")
        {
            if (!manifest.staticFile.empty())
            {
                candidates.push_back(moduleDir / manifest.staticFile);
            }
            candidates.push_back(moduleDir / "lib" / ("lib" + name + ".a"));
            candidates.push_back(moduleDir / ("lib" + name + ".a"));
        }
        else if (importLibrary)
        {
            if (!manifest.importLib.empty())
            {
                candidates.push_back(moduleDir / manifest.importLib);
            }
            candidates.push_back(moduleDir / "lib" / ("lib" + name + ".dll.a"));
            candidates.push_back(moduleDir / "lib" / (name + ".lib"));
        }
        else
        {
            const std::string sharedFile =
                !manifest.sharedFile.empty() ? manifest.sharedFile : manifest.file;
            if (!sharedFile.empty())
            {
                candidates.push_back(moduleDir / sharedFile);
            }
#if defined(_WIN32)
            candidates.push_back(moduleDir / "bin" / (name + ".dll"));
#elif defined(__APPLE__)
            candidates.push_back(moduleDir / "lib" / ("lib" + name + ".dylib"));
            candidates.push_back(moduleDir / "bin" / (name + ".dylib"));
#else
            candidates.push_back(moduleDir / "lib" / ("lib" + name + ".so"));
            candidates.push_back(moduleDir / "bin" / ("lib" + name + ".so"));
#endif
        }

        for (const fs::path& candidate : candidates)
        {
            std::error_code ec;
            if (fs::is_regular_file(candidate, ec) && !ec)
            {
                return candidate;
            }
        }

        return {};
    }

    fs::path FindCMakeModuleLibrary(const ModuleBuildContext& ctx,
                                   const std::string& name,
                                   const std::string& link,
                                   bool importLibrary)
    {
        std::vector<fs::path> dirs = {
            ctx.modulesRoot.parent_path() / "build/out/lib",
            ctx.modulesRoot.parent_path() / "build-linux/out/lib",
            ctx.modulesRoot.parent_path() / "build/out/bin",
            ctx.modulesRoot.parent_path() / "build-linux/out/bin"
        };
        for (const fs::path& dir : dirs)
        {
            ModuleManifest empty;
            const fs::path result =
                FindModuleLibrary(dir.parent_path(), empty, name, link, importLibrary);
            if (!result.empty())
            {
                return result;
            }
        }
        return {};
    }

    bool RunCompiler(const ModuleBuildContext& ctx,
                    const std::vector<std::string>& arguments,
                    std::string& error)
    {
        X_Y::Process::Options options;
        options.Capture = true;
        const X_Y::Process::Result result =
            X_Y::Process::Run(ctx.compiler, arguments, options);
        PrintText(result.Output);
        if (!result.Started)
        {
            error = "启动编译器失败: " + result.Error;
            return false;
        }
        if (!result.Error.empty())
        {
            error = "等待编译器失败: " + result.Error;
            return false;
        }
        if (result.ExitCode != 0)
        {
            error = "编译器退出码 " + std::to_string(result.ExitCode);
            return false;
        }
        return true;
    }

    bool PrepareModule(ModuleBuildContext& ctx, const std::string& requested,
                       const std::string& requestedVersion,
                       const std::string& requestedLink,
                       std::string& error)
    {
        fs::path moduleDir;
        ModuleManifest manifest;
        if (!FindModuleSource(ctx.modulesRoot, requested, requestedVersion,
                              moduleDir, manifest, error))
        {
            return false;
        }

        const std::string name = manifest.name;
        const std::string stackKey = name + ":" + requestedLink;
        if (std::find(ctx.stack.begin(), ctx.stack.end(), name) != ctx.stack.end())
        {
            error = "模块依赖成环: " + name;
            return false;
        }

        const fs::path moduleOut = ctx.outRoot / name;
        const fs::path includeDir = moduleOut / "include";
        const fs::path binDir = moduleOut / "bin";
        const fs::path libDir = moduleOut / "lib";
        const fs::path sourceRoot = moduleDir / manifest.sources;
        const bool hasSource = fs::is_directory(sourceRoot);
        const bool includeSources = requestedLink == "source";

        std::string link = requestedLink;
        if (link == "auto")
        {
            if (!FindModuleLibrary(moduleOut, manifest, name, "static", false).empty() ||
                !FindCMakeModuleLibrary(ctx, name, "static", false).empty())
            {
                link = "static";
            }
            else if (!FindModuleLibrary(moduleOut, manifest, name, "shared",
#if defined(_WIN32)
                                        true
#else
                                        false
#endif
                                        ).empty() ||
                     !FindCMakeModuleLibrary(ctx, name, "shared",
#if defined(_WIN32)
                                             true
#else
                                             false
#endif
                                             ).empty())
            {
                link = "shared";
            }
            else
            {
                link = "static";
            }
        }

        const std::string builtKey = name + ":" + link;
        if (ctx.built.count(builtKey) != 0)
        {
            return true;
        }

        ctx.stack.push_back(name);
        std::vector<std::pair<DependencySpec, PreparedModule>> dependencies;
        for (const std::string& token : SplitList(manifest.deps))
        {
            const DependencySpec dep = ParseDependency(token);
            if (!IsSafeComponent(dep.name))
            {
                error = "无效的模块依赖: " + token;
                ctx.stack.pop_back();
                return false;
            }
            const std::string depLink = requestedLink == "source" ? "source" : dep.link;
            if (!PrepareModule(ctx, dep.name, {}, depLink, error))
            {
                ctx.stack.pop_back();
                return false;
            }
            PreparedModule prepared;
            const fs::path depOut = ctx.outRoot / dep.name;
            ModuleManifest installed;
            if (!LoadModuleManifest(depOut, installed, error))
            {
                ctx.stack.pop_back();
                return false;
            }
            prepared.link = depLink;
            if (prepared.link == "auto")
            {
                if (!FindModuleLibrary(depOut, installed, dep.name, "static", false).empty())
                {
                    prepared.link = "static";
                }
                else
                {
                    prepared.link = "shared";
                }
            }
            if (prepared.link != "source")
            {
                prepared.library = FindModuleLibrary(depOut, installed, dep.name,
                    prepared.link,
#if defined(_WIN32)
                    prepared.link == "shared"
#else
                    false
#endif
                );
            }
            if (requestedLink != "source" && prepared.library.empty())
            {
                error = "依赖模块 " + dep.name + " 没有可链接的 " +
                        prepared.link + " 库";
                ctx.stack.pop_back();
                return false;
            }
            dependencies.emplace_back(dep, std::move(prepared));
        }

        std::error_code ec;
        fs::create_directories(includeDir, ec);
        fs::create_directories(binDir, ec);
        fs::create_directories(libDir, ec);
        if (ec)
        {
            error = "创建模块输出目录失败: " + moduleOut.string() +
                    ": " + ec.message();
            ctx.stack.pop_back();
            return false;
        }

        if (includeSources && hasSource)
        {
            if (!CopyDirectory(sourceRoot, moduleOut / "src", error))
            {
                ctx.stack.pop_back();
                return false;
            }
        }
        if (!CopyModuleHeaders(moduleDir, manifest, includeDir / name, error))
        {
            ctx.stack.pop_back();
            return false;
        }

        fs::path library;
        if (requestedLink != "source")
        {
            library = FindModuleLibrary(moduleOut, manifest, name, link,
#if defined(_WIN32)
                                        link == "shared"
#else
                                        false
#endif
                                        );
            if (library.empty())
            {
                library = FindModuleLibrary(moduleDir, manifest, name, link,
#if defined(_WIN32)
                                            link == "shared"
#else
                                            false
#endif
                                            );
            }
            if (library.empty())
            {
                const fs::path cmakeLib =
                    FindCMakeModuleLibrary(ctx, name, link,
#if defined(_WIN32)
                                           link == "shared"
#else
                                           false
#endif
                                           );
                if (!cmakeLib.empty())
                {
                    library = cmakeLib;
                }
            }
            if (library.empty())
            {
                if (!hasSource)
                {
                    error = "模块 " + name + " 没有 " + link +
                            " 预编译库，也没有源码可构建";
                    ctx.stack.pop_back();
                    return false;
                }

                std::vector<fs::path> sources;
                const std::vector<std::string> excludes = SplitList(manifest.exclude);
                fs::recursive_directory_iterator sourceIt(sourceRoot, ec);
                const fs::recursive_directory_iterator sourceEnd;
                for (; !ec && sourceIt != sourceEnd; sourceIt.increment(ec))
                {
                    if (!sourceIt->is_regular_file(ec) ||
                        sourceIt->path().extension() != ".cpp")
                    {
                        continue;
                    }
                    const std::string relative =
                        sourceIt->path().lexically_relative(moduleDir).generic_string();
                    const bool excluded = std::any_of(
                        excludes.begin(), excludes.end(),
                        [&](const std::string& exclude)
                        {
                            return relative == exclude ||
                                   relative.rfind(exclude, 0) == 0;
                        });
                    if (!excluded)
                    {
                        sources.push_back(sourceIt->path());
                    }
                }
                if (ec)
                {
                    error = "遍历模块源码失败: " + ec.message();
                    ctx.stack.pop_back();
                    return false;
                }
                std::sort(sources.begin(), sources.end());
                if (sources.empty())
                {
                    error = "模块 " + name + " 没有可编译的 .cpp";
                    ctx.stack.pop_back();
                    return false;
                }

                std::vector<std::string> common = {
                    "-std=c++20", "-O0", "-g",
                    "-finput-charset=UTF-8", "-fexec-charset=UTF-8"
                };
#if defined(_WIN32)
                common.insert(common.end(), {"-DXY_PLATFORM_WINDOWS", "-DUNICODE", "-D_UNICODE"});
#else
                common.push_back("-DXY_PLATFORM_LINUX");
                common.push_back("-fPIC");
#endif
                common.push_back("-I");
                common.push_back((moduleDir / manifest.include).string());
                common.push_back("-I");
                common.push_back(ctx.modulesRoot.string());
                common.push_back("-I");
                common.push_back(includeDir.string());
                common.push_back("-I");
                common.push_back(ctx.engineInclude.string());
                for (const auto& dependency : dependencies)
                {
                    common.push_back("-I");
                    common.push_back(
                        (ctx.outRoot / dependency.first.name / "include").string());
                }
                for (const std::string& define : SplitList(manifest.defines))
                {
                    common.push_back("-D" + define);
                }

                if (link == "static")
                {
                    const fs::path objectDir = ctx.tempRoot / name;
                    fs::create_directories(objectDir, ec);
                    if (ec)
                    {
                        error = "创建对象文件目录失败: " + ec.message();
                        ctx.stack.pop_back();
                        return false;
                    }
                    std::vector<std::string> objects;
                    for (std::size_t i = 0; i < sources.size(); ++i)
                    {
                        const fs::path object =
                            objectDir / (std::to_string(i) + ".o");
                        std::vector<std::string> args = common;
                        args.insert(args.end(), {"-c", sources[i].string(),
                                                 "-o", object.string()});
                        if (!RunCompiler(ctx, args, error))
                        {
                            ctx.stack.pop_back();
                            return false;
                        }
                        objects.push_back(object.string());
                    }
                    const fs::path archive = libDir / ("lib" + name + ".a");
                    std::vector<std::string> args = {"rcs", archive.string()};
                    args.insert(args.end(), objects.begin(), objects.end());
                    X_Y::Process::Options options;
                    options.Capture = true;
                    const X_Y::Process::Result result =
                        X_Y::Process::Run("ar", args, options);
                    PrintText(result.Output);
                    if (!result.Ok())
                    {
                        error = "创建静态库失败: " +
                                (result.Error.empty() ?
                                     std::to_string(result.ExitCode) : result.Error);
                        ctx.stack.pop_back();
                        return false;
                    }
                    library = archive;
                }
                else
                {
#if defined(_WIN32)
                    const fs::path shared = binDir / (name + ".dll");
                    const fs::path import = libDir / ("lib" + name + ".dll.a");
#elif defined(__APPLE__)
                    const fs::path shared = binDir / ("lib" + name + ".dylib");
                    const fs::path import = shared;
#else
                    const fs::path shared = binDir / ("lib" + name + ".so");
                    const fs::path import = shared;
#endif
                    std::vector<std::string> args = common;
                    args.push_back("-shared");
                    args.push_back("-o");
                    args.push_back(shared.string());
#if defined(_WIN32)
                    args.push_back("-Wl,--out-implib," + import.string());
#endif
                    for (const auto& source : sources)
                    {
                        args.push_back(source.string());
                    }
                    for (const auto& dependency : dependencies)
                    {
                        if (!dependency.second.library.empty())
                        {
                            args.push_back(dependency.second.library.string());
                        }
                    }
                    args.push_back("-L");
                    args.push_back(ctx.libRoot.string());
                    for (const std::string& libDir : SplitList(manifest.libDirs))
                    {
                        args.push_back("-L");
                        args.push_back((moduleDir / libDir).string());
                    }
                    for (const std::string& lib : SplitList(manifest.libs))
                    {
                        args.push_back("-l" + lib);
                    }
#if defined(_WIN32)
                    for (const std::string& lib : SplitList(manifest.libsWindows))
                    {
                        args.push_back("-l" + lib);
                    }
#else
                    for (const std::string& lib : SplitList(manifest.libsLinux))
                    {
                        args.push_back("-l" + lib);
                    }
#endif
                    if (!RunCompiler(ctx, args, error))
                    {
                        ctx.stack.pop_back();
                        return false;
                    }
                    library =
#if defined(_WIN32)
                        import;
#else
                        shared;
#endif
                }
            }

            if (!library.empty() && library.parent_path() != libDir)
            {
                fs::path destination;
                if (link == "static")
                {
                    destination = libDir / ("lib" + name + ".a");
                }
                else
                {
#if defined(_WIN32)
                    destination = libDir / ("lib" + name + ".dll.a");
#else
                    destination = binDir / library.filename();
#endif
                }
                fs::create_directories(destination.parent_path(), ec);
                if (ec)
                {
                    error = "创建库目录失败: " + ec.message();
                    ctx.stack.pop_back();
                    return false;
                }
                if (library != destination)
                {
                    fs::copy_file(library, destination,
                                  fs::copy_options::overwrite_existing, ec);
                    if (ec)
                    {
                        error = "复制模块库失败: " + library.string() +
                                ": " + ec.message();
                        ctx.stack.pop_back();
                        return false;
                    }
                }
                library = destination;
            }

            if (link == "shared")
            {
                fs::path runtime = FindModuleLibrary(moduleOut, manifest, name,
                                                     "shared", false);
                if (runtime.empty())
                {
                    runtime = FindModuleLibrary(moduleDir, manifest, name,
                                                "shared", false);
                }
                if (runtime.empty())
                {
                    runtime = FindCMakeModuleLibrary(ctx, name, "shared", false);
                }
                if (!runtime.empty())
                {
#if defined(_WIN32)
                    const fs::path runtimeDestination = binDir / (name + ".dll");
#elif defined(__APPLE__)
                    const fs::path runtimeDestination = binDir / ("lib" + name + ".dylib");
#else
                    const fs::path runtimeDestination = binDir / ("lib" + name + ".so");
#endif
                    if (runtime != runtimeDestination)
                    {
                        fs::copy_file(runtime, runtimeDestination,
                                      fs::copy_options::overwrite_existing, ec);
                        if (ec)
                        {
                            error = "复制共享库运行文件失败: " + ec.message();
                            ctx.stack.pop_back();
                            return false;
                        }
                    }
                }
            }
        }

        std::ofstream package(moduleOut / "package.xy");
        if (!package)
        {
            error = "写 package.xy 失败: " + moduleOut.string();
            ctx.stack.pop_back();
            return false;
        }
        std::string resolvedDeps;
        if (requestedLink == "source")
        {
            resolvedDeps = manifest.deps;
        }
        else
        {
            for (const auto& dependency : dependencies)
            {
                if (!resolvedDeps.empty())
                {
                    resolvedDeps += ", ";
                }
                resolvedDeps += dependency.first.name + ":" +
                                dependency.second.link;
            }
        }

        const fs::path staticArtifact = libDir / ("lib" + name + ".a");
#if defined(_WIN32)
        const fs::path sharedRuntime = binDir / (name + ".dll");
        const fs::path sharedImport = libDir / ("lib" + name + ".dll.a");
#elif defined(__APPLE__)
        const fs::path sharedRuntime = binDir / ("lib" + name + ".dylib");
        const fs::path sharedImport = sharedRuntime;
#else
        const fs::path sharedRuntime = binDir / ("lib" + name + ".so");
        const fs::path sharedImport = sharedRuntime;
#endif
        package << "name = " << name << '\n'
                << "version = " << manifest.version << '\n'
                << "kind = library\n"
                << "abi = 1\n"
                << "source = " << (includeSources && hasSource ? "src" : "") << '\n'
                << "include = include\n"
                << "headers = include/" << name << '\n'
                << "file = "
                << (fs::exists(sharedRuntime) ? "bin/" + sharedRuntime.filename().string() : "")
                << '\n'
                << "static_file = "
                << (fs::exists(staticArtifact) ? "lib/" + staticArtifact.filename().string() : "")
                << '\n'
                << "shared_file = "
                << (fs::exists(sharedRuntime) ? "bin/" + sharedRuntime.filename().string() : "")
                << '\n'
                << "import_lib = "
                << (fs::exists(sharedImport) ? "lib/" + sharedImport.filename().string() : "")
                << '\n'
                << "deps = " << resolvedDeps << '\n'
                << "libs = " << manifest.libs << '\n'
                << "libs_windows = " << manifest.libsWindows << '\n'
                << "libs_linux = " << manifest.libsLinux << '\n'
                << "libDirs = " << manifest.libDirs << '\n'
                << "defines = " << manifest.defines << '\n';
        package.close();
        if (!package)
        {
            error = "写 package.xy 失败: " + moduleOut.string();
            ctx.stack.pop_back();
            return false;
        }

        ctx.built.insert(builtKey);
        ctx.stack.pop_back();
        std::printf("prepared module %s %s (%s) -> %s\n",
                    name.c_str(), manifest.version.c_str(), link.c_str(),
                    moduleOut.string().c_str());
        return true;
    }

    int CmdModule(int argc, char** argv, const fs::path& exeDir,
                  const BuildEnv& env)
    {
        std::string requested;
        std::string version;
        std::string link = "auto";
        fs::path modulesRoot;
        fs::path outRoot = env.modules;
        const char* cxxOption = nullptr;

        for (int i = 2; i < argc; ++i)
        {
            const std::string_view option = argv[i];
            if (option == "--modules-root")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy module: --modules-root 缺少值\n");
                    return 2;
                }
                modulesRoot = argv[++i];
            }
            else if (option == "--repo")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy module: --repo 缺少值\n");
                    return 2;
                }
                modulesRoot = argv[++i];
            }
            else if (option == "--out")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy module: --out 缺少值\n");
                    return 2;
                }
                outRoot = argv[++i];
            }
            else if (option == "--cxx")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy module: --cxx 缺少值\n");
                    return 2;
                }
                cxxOption = argv[++i];
            }
            else if (option == "--link")
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "xy module: --link 缺少值\n");
                    return 2;
                }
                link = argv[++i];
                if (link != "auto" && link != "source" &&
                    link != "static" && link != "shared")
                {
                    std::fprintf(stderr,
                                 "xy module: --link 必须是 auto、source、static 或 shared\n");
                    return 2;
                }
            }
            else if (!option.empty() && option[0] != '-')
            {
                if (requested.empty())
                {
                    requested = option;
                }
                else if (version.empty())
                {
                    version = option;
                }
                else
                {
                    std::fprintf(stderr, "xy module: 多余的位置参数 '%s'\n", argv[i]);
                    return 2;
                }
            }
            else
            {
                std::fprintf(stderr, "xy module: 未知参数 '%s'\n", argv[i]);
                return 2;
            }
        }

        if (requested.empty())
        {
            std::fprintf(stderr,
                         "用法: xy module <name> [version] [--link auto|source|static|shared] "
                         "[--repo <dir>] [--out <dir>] [--cxx <编译器>]\n");
            return 2;
        }

        if (modulesRoot.empty())
        {
            modulesRoot = env.source;
        }
        if (modulesRoot.empty())
        {
            std::fprintf(stderr,
                         "xy module: 找不到模块仓库，用 --repo 或 enve.source 指定\n");
            return 2;
        }

        fs::path engineInclude = exeDir / "../../../Engine/include";
        std::error_code ec;
        if (!fs::is_directory(engineInclude, ec) || ec)
        {
            engineInclude = fs::current_path() / "X_Y/Engine/include";
        }

        ModuleBuildContext ctx;
        ctx.modulesRoot = modulesRoot;
        ctx.outRoot = outRoot;
        ctx.libRoot = env.lib;
        ctx.tempRoot = env.temp / "modules";
        ctx.engineInclude = engineInclude;
        ctx.compiler = ResolveCompiler(cxxOption);

        std::string error;
        if (!PrepareModule(ctx, requested, version, link, error))
        {
            std::fprintf(stderr, "xy module: %s\n", error.c_str());
            return 3;
        }
        return 0;
    }

    int CmdEnv(const BuildEnv& env)
    {
        std::printf("config  = %s\n",
                    env.config.empty() ? "(default)" : env.config.string().c_str());
        std::printf("root    = %s\n", env.root.string().c_str());
        std::printf("bin     = %s\n", env.bin.string().c_str());
        std::printf("modules = %s\n", env.modules.string().c_str());
        std::printf("programs= %s\n", env.programs.string().c_str());
        std::printf("lib     = %s\n", env.lib.string().c_str());
        std::printf("temp    = %s\n", env.temp.string().c_str());
        std::printf("source  = %s\n", env.source.string().c_str());
        return 0;
    }

    void Usage()
    {
        std::printf(
            "xy —— X_Y 最小构建器 / 仓库安装入口\n"
            "\n"
            "用法:\n"
            "  xy build   <源目录> [--out <目录>] [--include <Engine/include>] [--cxx <编译器>]\n"
            "  xy module  <name> [version] [--link auto|source|static|shared]\n"
            "             [--repo <模块仓库>] [--out <modules 目录>] [--cxx <编译器>]\n"
            "  xy install <name> [version] [--repo <仓库>] [--root <安装根>] [--out <目录>]\n"
            "             [--build] [--force] [--include <Engine/include>] [--cxx <编译器>]\n"
            "             [--x-y <X_Y.exe>] [--pkg <pkg.dll>]\n"
            "  xy doctor  [--repo <仓库>] [--include <Engine/include>] [--cxx <编译器>]\n"
            "             [--x-y <X_Y.exe>] [--pkg <pkg.dll>]\n"
            "  xy env\n"
            "  xy help\n"
            "\n"
            "install 获取可运行包；module 准备开发模块；build 只构建当前包。\n"
            "module 默认优先复用静态库，其次共享库；缺少所选产物时从源码构建。\n"
            "build 按 package.xy 的 kind 链接：program/module 为共享库，executable 为独立程序。\n");
    }

    int BuildPackage(const fs::path& source,
                     const fs::path& outputRoot,
                     const fs::path& include,
                     const std::string& compiler,
                     bool printNext,
                     bool useConfiguredProgramOutput,
                     const BuildEnv& env)
    {
        Manifest manifest;
        std::string error;
        if (!LoadManifest(source / "package.xy", manifest, error))
        {
            std::fprintf(stderr, "xy build: %s\n", error.c_str());
            return 3;
        }

        const bool executable = manifest.kind == "executable";
        if (!executable && include.empty())
        {
            std::fprintf(stderr,
                         "xy build: 构建 program/module 需要 Engine/include，用 --include 或 XY_ENGINE_INCLUDE 指定\n");
            return 3;
        }

        std::vector<fs::path> sources;
        if (!CollectSources(source, sources, error))
        {
            std::fprintf(stderr, "xy build: %s\n", error.c_str());
            return 3;
        }

        const fs::path packageRoot =
            executable && useConfiguredProgramOutput ? env.programs : outputRoot;
        const fs::path destination = packageRoot / manifest.name / manifest.version;
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
        if (!executable)
        {
            arguments.push_back("-shared");
        }
        arguments.push_back("-O0");
        arguments.push_back("-g");
        arguments.push_back("-finput-charset=UTF-8");
        arguments.push_back("-fexec-charset=UTF-8");
#if !defined(_WIN32)
        if (!executable)
        {
            arguments.push_back("-fPIC");
        }
#endif
        if (!include.empty())
        {
            arguments.push_back("-I");
            arguments.push_back(include.string());
        }
        arguments.push_back("-I");
        arguments.push_back(env.source.string());

        for (const std::string& extra : SplitList(manifest.includes))
        {
            arguments.push_back("-I");
            arguments.push_back((source / extra).string());
        }

        for (const std::string& extra : SplitList(manifest.libDirs))
        {
            arguments.push_back("-L");
            arguments.push_back((source / extra).string());
        }

        arguments.push_back("-L");
        arguments.push_back(env.lib.string());

        arguments.push_back("-o");
        arguments.push_back(target.string());
        for (const auto& path : sources)
        {
            arguments.push_back(path.string());
        }

        std::map<std::string, std::string> requestedLinks;
        std::vector<DependencySpec> rootDependencies;
        for (const std::string& token : SplitList(manifest.deps))
        {
            DependencySpec dep = ParseDependency(token);
            if (dep.link != "auto")
            {
                const auto existing = requestedLinks.find(dep.name);
                if (existing != requestedLinks.end() && existing->second != dep.link)
                {
                    std::fprintf(stderr,
                                 "xy build: 依赖 %s 声明了冲突的链接类型\n",
                                 dep.name.c_str());
                    return 3;
                }
                requestedLinks[dep.name] = dep.link;
            }
            rootDependencies.push_back(std::move(dep));
        }

        std::set<std::string> addedDependencies;
        std::function<bool(const DependencySpec&)> addDependency =
            [&](const DependencySpec& requested) -> bool
        {
            if (addedDependencies.count(requested.name) != 0)
            {
                return true;
            }
            const fs::path depOut = env.modules / requested.name;
            ModuleManifest module;
            std::string moduleError;
            if (!LoadModuleManifest(depOut, module, moduleError))
            {
                std::fprintf(stderr,
                             "xy build: 缺少模块 %s（请先运行 xy module %s）：%s\n",
                             requested.name.c_str(), requested.name.c_str(),
                             moduleError.c_str());
                return false;
            }

            arguments.push_back("-I");
            arguments.push_back((depOut / module.include).string());

            std::string link = requested.link;
            const auto overrideLink = requestedLinks.find(requested.name);
            if (overrideLink != requestedLinks.end())
            {
                link = overrideLink->second;
            }
            if (link == "auto")
            {
                link = !FindModuleLibrary(depOut, module, requested.name,
                                          "static", false).empty()
                           ? "static"
                           : "shared";
            }
            const fs::path library = FindModuleLibrary(
                depOut, module, requested.name, link,
#if defined(_WIN32)
                link == "shared"
#else
                false
#endif
            );
            if (library.empty())
            {
                std::fprintf(stderr,
                             "xy build: 模块 %s 没有可用的 %s 库（请运行 xy module %s --link %s）\n",
                             requested.name.c_str(), link.c_str(),
                             requested.name.c_str(), link.c_str());
                return false;
            }

            addedDependencies.insert(requested.name);
            arguments.push_back(library.string());

            for (const std::string& libDir : SplitList(module.libDirs))
            {
                arguments.push_back("-L");
                arguments.push_back((depOut / libDir).string());
            }
            for (const std::string& lib : SplitList(module.libs))
            {
                arguments.push_back("-l" + lib);
            }
#if defined(_WIN32)
            for (const std::string& lib : SplitList(module.libsWindows))
            {
                arguments.push_back("-l" + lib);
            }
#else
            for (const std::string& lib : SplitList(module.libsLinux))
            {
                arguments.push_back("-l" + lib);
            }
#endif
            for (const std::string& dependencyToken : SplitList(module.deps))
            {
                DependencySpec dependency = ParseDependency(dependencyToken);
                if (!addDependency(dependency))
                {
                    return false;
                }
            }
            return true;
        };

        for (const DependencySpec& dependency : rootDependencies)
        {
            if (!addDependency(dependency))
            {
                return 3;
            }
        }

        for (const std::string& lib : SplitList(manifest.libs))
        {
            arguments.push_back("-l" + lib);
        }
#if defined(_WIN32)
        for (const std::string& lib : SplitList(manifest.libsWindows))
        {
            arguments.push_back("-l" + lib);
        }
#else
        for (const std::string& lib : SplitList(manifest.libsLinux))
        {
            arguments.push_back("-l" + lib);
        }
#endif
#if !defined(_WIN32)
        arguments.push_back("-pthread");
#endif
#if defined(_WIN32)
        if (!executable)
        {
            arguments.push_back("-Wl,--exclude-all-symbols");
        }
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

        std::error_code copyEc;
        fs::remove(destination / "package.xy", copyEc);
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
            if (executable)
            {
                std::printf("next: X_Y --run exec \"%s\" [参数...]\n",
                            target.string().c_str());
            }
            else
            {
                std::printf("next: X_Y --module pkg.dll --run pkg install %s --root Modules\n",
                            destination.string().c_str());
            }
        }
        return 0;
    }

    int CmdBuild(int argc, const char* const* argv, const fs::path& exeDir,
                 const BuildEnv& env)
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "用法: xy build <源目录> [选项]\n");
            return 2;
        }

        const fs::path source = argv[2];
        fs::path outputRoot = env.temp / "out";
        bool useConfiguredProgramOutput = true;
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
                useConfiguredProgramOutput = false;
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
        return BuildPackage(source, outputRoot, include,
                            ResolveCompiler(compilerOption), true,
                            useConfiguredProgramOutput, env);
    }

    int CmdInstall(int argc, const char* const* argv, const fs::path& exeDir,
                   const BuildEnv& env)
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
            "XY_REPO", fs::current_path() / "repo");
        const fs::path root = ResolvePath(
            rootArgument.empty() ? nullptr : rootArgument.c_str(),
            "XY_MODULES", env.modules);
        const fs::path outputRoot =
            outArgument.empty() ? (env.temp / "out") : fs::path(outArgument);

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
            std::printf("building %s %s from %s\n",
                        selected.manifest.name.c_str(),
                        selected.manifest.version.c_str(),
                        selected.directory.string().c_str());

            const int built = BuildPackage(
                selected.directory,
                outputRoot,
                include,
                ResolveCompiler(compilerArgument.empty() ? nullptr : compilerArgument.c_str()),
                false,
                false,
                env);
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

    int CmdDoctor(int argc, const char* const* argv, const fs::path& exeDir,
                  const BuildEnv& env)
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
            "XY_REPO", env.source);
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
    const BuildEnv env = LoadBuildEnv(exeDir);
    SetBuildTempDir(env);

    if (argc < 2)
    {
        Usage();
        return 2;
    }

    const std::string_view command = argv[1];
    if (command == "build")
    {
        return CmdBuild(argc, argv, exeDir, env);
    }
    if (command == "module")
    {
        return CmdModule(argc, argv, exeDir, env);
    }
    if (command == "install")
    {
        return CmdInstall(argc, argv, exeDir, env);
    }
    if (command == "doctor")
    {
        return CmdDoctor(argc, argv, exeDir, env);
    }
    if (command == "env")
    {
        return CmdEnv(env);
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
