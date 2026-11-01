#pragma once
/*
EngineContainer.h —— 单线程 Engine 容器。

它持有：
  - 一个根 Engine（owner = "root"）
  - 若干子 Engine，每个有自己的 host / services / commands / modules

现在只做 create/list/destroy；跨 engine 服务查找和线程都还没做。
*/

#include "Host.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace X_Y
{
    class EngineContainer
    {
    public:
        struct Info
        {
            std::string name;
            std::size_t modules = 0;
            std::size_t commands = 0;
        };

        EngineContainer();
        ~EngineContainer()
        {
            // 共享表只借指针；先清表，再让 children/root 析构。
            sharedProviders_.clear();
            shared_.Clear();
            sharedCommandProviders_.clear();
            sharedCommands_.Clear();
        }

        EngineContainer(const EngineContainer&) = delete;
        EngineContainer& operator=(const EngineContainer&) = delete;

        Engine& Root() { return root_; }

        // root 或子 Engine；找不到返回 nullptr。
        Engine* Find(const std::string& name);

        // 创建并 Start 一个子 Engine（不带内建命令）。
        // 失败返回 nullptr，error 写原因。
        Engine* Create(const std::string& name, std::string& error);

        // 销毁子 Engine；root 不能删，provider 还有共享服务时也不能删。
        bool Destroy(const std::string& name, std::string& error);

        // 把某个 engine 的本地服务提升到容器共享表。
        // 共享期间 provider engine 不能被销毁。
        bool Share(const std::string& engineName, const std::string& service,
                   std::string& error);

        // 从共享表摘掉服务；之后 provider 才能销毁/卸载。
        bool Unshare(const std::string& service, std::string& error);

        // 容器共享表查找；没有返回 nullptr。
        void* GetShared(const std::string& service) const;

        // engine 是否还有服务在共享表里。
        bool HasSharedServices(const Engine* engine) const;

        struct SharedInfo
        {
            std::string service;
            std::string provider;
        };

        // 当前共享表，按服务名字典序。
        std::vector<SharedInfo> SharedList() const;

        // 命令共享（和服务的 shared 同层）：local -> shared commands -> root。
        bool ShareCommand(const std::string& engineName, const std::string& command,
                          std::string& error);
        bool UnshareCommand(const std::string& command, std::string& error);

        const XyCommandV1* GetSharedCommand(const std::string& command) const;
        std::string SharedCommandOwner(const std::string& command) const;
        bool HasSharedCommands(const Engine* engine) const;

        struct SharedCommandInfo
        {
            std::string command;
            std::string provider;
        };

        std::vector<SharedCommandInfo> SharedCommandList() const;

        // root + 子 engine，按创建顺序。
        std::vector<Info> List() const;

        std::size_t Count() const { return children_.size(); }

    private:
        Engine root_;
        std::vector<std::unique_ptr<Engine>> children_;
        Services shared_;
        std::map<std::string, Engine*> sharedProviders_;
        CommandTable sharedCommands_;
        std::map<std::string, Engine*> sharedCommandProviders_;
    };
}
