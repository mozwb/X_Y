#include "EngineContainer.h"

#include <algorithm>
#include <utility>

namespace X_Y
{
    EngineContainer::EngineContainer()
    {
        root_.name = "root";
        root_.container = this;
    }

    Engine* EngineContainer::Find(const std::string& name)
    {
        if (root_.name == name)
        {
            return &root_;
        }

        const auto it = std::find_if(
            children_.begin(), children_.end(),
            [&name](const std::unique_ptr<Engine>& engine)
            {
                return engine->name == name;
            });
        return it == children_.end() ? nullptr : it->get();
    }

    Engine* EngineContainer::Create(const std::string& name, std::string& error)
    {
        error.clear();

        if (name.empty())
        {
            error = "engine 名字不能为空";
            return nullptr;
        }
        if (name == "root")
        {
            error = "root engine 已存在";
            return nullptr;
        }
        if (Find(name) != nullptr)
        {
            error = "engine 已存在: " + name;
            return nullptr;
        }

        auto engine = std::make_unique<Engine>();
        engine->name = name;
        engine->container = this;
        engine->Start(false);

        Engine* raw = engine.get();
        children_.push_back(std::move(engine));
        return raw;
    }

    bool EngineContainer::Destroy(const std::string& name, std::string& error)
    {
        error.clear();

        if (name == "root")
        {
            error = "root engine 不能销毁";
            return false;
        }

        const auto it = std::find_if(
            children_.begin(), children_.end(),
            [&name](const std::unique_ptr<Engine>& engine)
            {
                return engine->name == name;
            });
        if (it == children_.end())
        {
            error = "没有 engine: " + name;
            return false;
        }

        if (HasSharedServices(it->get()) || HasSharedCommands(it->get()))
        {
            error = "engine " + name + " 还有共享服务/命令，先 unshare";
            return false;
        }

        // Engine 析构会先销毁 modules，再销毁 services/commands。
        children_.erase(it);
        return true;
    }

    bool EngineContainer::Share(const std::string& engineName,
                                const std::string& service,
                                std::string& error)
    {
        error.clear();

        Engine* provider = Find(engineName);
        if (provider == nullptr)
        {
            error = "没有 engine: " + engineName;
            return false;
        }
        if (service.empty())
        {
            error = "服务名不能为空";
            return false;
        }

        void* ptr = provider->services.Get(service);
        if (ptr == nullptr)
        {
            error = "engine " + engineName + " 没有服务: " + service;
            return false;
        }
        if (shared_.Get(service) != nullptr)
        {
            error = "服务已在共享表: " + service;
            return false;
        }

        const std::string owner = "shared:" + provider->name;
        if (!shared_.Add(service, ptr, owner))
        {
            error = "登记共享服务失败: " + service;
            return false;
        }

        sharedProviders_[service] = provider;
        return true;
    }

    bool EngineContainer::Unshare(const std::string& service, std::string& error)
    {
        error.clear();

        if (!shared_.Remove(service))
        {
            error = "共享表里没有服务: " + service;
            return false;
        }
        sharedProviders_.erase(service);
        return true;
    }

    void* EngineContainer::GetShared(const std::string& service) const
    {
        return shared_.Get(service);
    }

    bool EngineContainer::HasSharedServices(const Engine* engine) const
    {
        for (const auto& [name, provider] : sharedProviders_)
        {
            (void)name;
            if (provider == engine)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<EngineContainer::SharedInfo> EngineContainer::SharedList() const
    {
        std::vector<SharedInfo> out;
        for (const auto& service : shared_.Names())
        {
            const auto it = sharedProviders_.find(service);
            if (it == sharedProviders_.end() || it->second == nullptr)
            {
                continue;
            }

            SharedInfo info;
            info.service = service;
            info.provider = it->second->name;
            out.push_back(std::move(info));
        }
        return out;
    }

    bool EngineContainer::ShareCommand(const std::string& engineName,
                                       const std::string& command,
                                       std::string& error)
    {
        error.clear();

        Engine* provider = Find(engineName);
        if (provider == nullptr)
        {
            error = "没有 engine: " + engineName;
            return false;
        }
        if (command.empty())
        {
            error = "命令名不能为空";
            return false;
        }

        const XyCommandV1* cmd = provider->commands.Find(command);
        if (cmd == nullptr)
        {
            error = "engine " + engineName + " 没有命令: " + command;
            return false;
        }
        if (sharedCommands_.Find(command) != nullptr)
        {
            error = "命令已在共享表: " + command;
            return false;
        }

        const std::string owner = "shared:" + provider->name;
        if (!sharedCommands_.Add(cmd, owner))
        {
            error = "登记共享命令失败: " + command;
            return false;
        }

        sharedCommandProviders_[command] = provider;
        return true;
    }

    bool EngineContainer::UnshareCommand(const std::string& command, std::string& error)
    {
        error.clear();

        if (!sharedCommands_.Remove(command))
        {
            error = "共享表里没有命令: " + command;
            return false;
        }
        sharedCommandProviders_.erase(command);
        return true;
    }

    const XyCommandV1* EngineContainer::GetSharedCommand(const std::string& command) const
    {
        return sharedCommands_.Find(command);
    }

    std::string EngineContainer::SharedCommandOwner(const std::string& command) const
    {
        return sharedCommands_.OwnerOf(command);
    }

    bool EngineContainer::HasSharedCommands(const Engine* engine) const
    {
        for (const auto& [name, provider] : sharedCommandProviders_)
        {
            (void)name;
            if (provider == engine)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<EngineContainer::SharedCommandInfo> EngineContainer::SharedCommandList() const
    {
        std::vector<SharedCommandInfo> out;
        for (const auto& command : sharedCommands_.Names())
        {
            const auto it = sharedCommandProviders_.find(command);
            if (it == sharedCommandProviders_.end() || it->second == nullptr)
            {
                continue;
            }

            SharedCommandInfo info;
            info.command = command;
            info.provider = it->second->name;
            out.push_back(std::move(info));
        }
        return out;
    }

    std::vector<EngineContainer::Info> EngineContainer::List() const
    {
        std::vector<Info> out;
        out.reserve(children_.size() + 1);

        Info rootInfo;
        rootInfo.name = root_.name;
        rootInfo.modules = root_.modules ? root_.modules->Count() : 0;
        rootInfo.commands = root_.commands.Size();
        out.push_back(std::move(rootInfo));

        for (const auto& engine : children_)
        {
            Info info;
            info.name = engine->name;
            info.modules = engine->modules ? engine->modules->Count() : 0;
            info.commands = engine->commands.Size();
            out.push_back(std::move(info));
        }

        return out;
    }
}
