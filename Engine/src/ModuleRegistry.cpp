#include "ModuleRegistry.h"

#include "CommandTable.h"
#include "Services.h"

#include <algorithm>
#include <string>
#include <utility>

namespace X_Y
{
    namespace
    {
        // 装载 DLL 期间，host 的 add_service 要靠 currentModule 标注归属。
        // 作用域退出自动清空，异常/提前 return 也不会把 owner 留在上下文里。
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

        bool ValidCommand(const XyCommandV1* cmd)
        {
            return cmd != nullptr &&
                   cmd->size >= sizeof(XyCommandV1) &&
                   cmd->name != nullptr &&
                   cmd->name[0] != '\0' &&
                   cmd->invoke != nullptr;
        }
    }

    ModuleRegistry::ModuleRegistry(const XyHostV1& host,
                                   Services& services,
                                   CommandTable& commands,
                                   std::string& currentOwner)
        : host_(&host)
        , services_(services)
        , commands_(commands)
        , currentOwner_(currentOwner)
    {
    }

    ModuleRegistry::~ModuleRegistry()
    {
        UnloadAll();
    }

    std::string ModuleRegistry::BaseName(const std::string& path)
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

    ModuleRegistry::Module* ModuleRegistry::Find(const std::string& name)
    {
        const auto it = std::find_if(
            modules_.begin(), modules_.end(),
            [&name](const Module& module) { return module.name == name; });
        return it == modules_.end() ? nullptr : &*it;
    }

    bool ModuleRegistry::IsLoaded(const std::string& name) const
    {
        return std::any_of(
            modules_.begin(), modules_.end(),
            [&name](const Module& module) { return module.name == name; });
    }

    int ModuleRegistry::InvokeProgram(void* self, int argc, const char* const* argv)
    {
        auto* binding = static_cast<ProgramBinding*>(self);
        if (binding == nullptr || binding->entry == nullptr)
        {
            return 1;
        }
        return binding->entry(binding->host, argc, argv);
    }

    bool ModuleRegistry::Load(const std::string& path, std::string& error)
    {
        error.clear();

        const std::string name = BaseName(path);
        if (name.empty())
        {
            error = "无法从路径得到模块名: " + path;
            return false;
        }
        if (Find(name) != nullptr)
        {
            error = "模块已装载: " + name;
            return false;
        }

        Platform::Library library;
        if (!library.Open(path, error))
        {
            return false;
        }

        auto version = reinterpret_cast<xy_abi_version_v1_fn>(
            library.Symbol(XY_ABI_VERSION_ENTRY));
        if (version == nullptr)
        {
            error = name + " 没有导出 " XY_ABI_VERSION_ENTRY;
            return false;
        }

        const uint32_t abi = version();
        if (abi != XY_ABI_VERSION)
        {
            error = "ABI 版本不匹配: " + name + " 是 " + std::to_string(abi) +
                    "，内核是 " + std::to_string(XY_ABI_VERSION);
            return false;
        }

        auto program = reinterpret_cast<xy_program_v1_fn>(
            library.Symbol(XY_PROGRAM_ENTRY));
        auto commandEntry = reinterpret_cast<xy_commands_v1_fn>(
            library.Symbol(XY_COMMANDS_ENTRY));
        if (program == nullptr && commandEntry == nullptr)
        {
            error = name + " 既没有 " XY_PROGRAM_ENTRY "，也没有 " XY_COMMANDS_ENTRY;
            return false;
        }

        const std::string owner = name;
        std::vector<std::string> added;
        bool ok = true;

        if (commandEntry != nullptr)
        {
            OwnerScope scope(currentOwner_, owner);
            const XyCommandV1* const* table = commandEntry(host_);
            for (const XyCommandV1* const* it = table; it != nullptr && *it != nullptr; ++it)
            {
                const XyCommandV1* cmd = *it;
                if (!ValidCommand(cmd))
                {
                    error = name + " 的命令表里有非法命令";
                    ok = false;
                    break;
                }
                if (!commands_.Add(cmd, owner))
                {
                    error = "登记命令失败（重名或非法）: " + std::string(cmd->name);
                    ok = false;
                    break;
                }
                added.push_back(cmd->name);
            }
        }

        std::unique_ptr<ProgramBinding> binding;
        std::unique_ptr<XyCommandV1> programCommand;

        if (ok && program != nullptr &&
            std::find(added.begin(), added.end(), name) == added.end())
        {
            binding = std::make_unique<ProgramBinding>();
            binding->entry = program;
            binding->host = host_;
            binding->name = name;

            programCommand = std::make_unique<XyCommandV1>();
            programCommand->size = sizeof(XyCommandV1);
            programCommand->name = binding->name.c_str();
            programCommand->help = "运行微程序";
            programCommand->invoke = &ModuleRegistry::InvokeProgram;
            programCommand->self = binding.get();

            if (!commands_.Add(programCommand.get(), owner))
            {
                error = "微程序命令名冲突: " + name;
                ok = false;
            }
            else
            {
                added.push_back(name);
            }
        }

        if (!ok)
        {
            commands_.RemoveByOwner(owner);
            for (const auto& service : services_.OwnedBy(owner))
            {
                services_.Remove(service);
            }
            return false;
        }

        Module module;
        module.name = name;
        module.path = path;
        module.owner = owner;
        module.library = std::move(library);
        module.program = program;
        module.binding = std::move(binding);
        module.programCommand = std::move(programCommand);
        module.commands = std::move(added);
        modules_.push_back(std::move(module));
        return true;
    }

    bool ModuleRegistry::Unload(const std::string& name, int& leaks, std::string& error)
    {
        error.clear();
        leaks = 0;

        const auto it = std::find_if(
            modules_.begin(), modules_.end(),
            [&name](const Module& module) { return module.name == name; });
        if (it == modules_.end())
        {
            error = "没有装载模块: " + name;
            return false;
        }

        // 先摘命令，再摘服务，最后才销毁 Library —— 否则命令表会悬垂。
        commands_.RemoveByOwner(it->owner);
        for (const auto& service : services_.OwnedBy(it->owner))
        {
            services_.Remove(service);
            ++leaks;
        }

        modules_.erase(it);
        return true;
    }

    int ModuleRegistry::UnloadAll()
    {
        int leaks = 0;
        for (auto it = modules_.rbegin(); it != modules_.rend(); ++it)
        {
            commands_.RemoveByOwner(it->owner);
            for (const auto& service : services_.OwnedBy(it->owner))
            {
                services_.Remove(service);
                ++leaks;
            }
        }
        modules_.clear();
        return leaks;
    }

    std::vector<ModuleRegistry::Info> ModuleRegistry::List() const
    {
        std::vector<Info> out;
        out.reserve(modules_.size());
        for (const auto& module : modules_)
        {
            Info info;
            info.name = module.name;
            info.path = module.path;
            info.owner = module.owner;
            info.hasProgram = module.program != nullptr;
            info.commands = module.commands;
            out.push_back(std::move(info));
        }
        return out;
    }
}
