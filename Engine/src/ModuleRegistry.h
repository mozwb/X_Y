#pragma once
/*
ModuleRegistry.h —— 内核的模块生命周期：装载 → 验 ABI → 登记 → 卸载。

它只认得 DLL 的入口符号，不认得包、版本、store：
  - 包/版本是 pkg 的事
  - 编译是 xy 的事
这里只回答“这个 DLL 能不能装、装进来有什么、怎么干净地卸掉”。

【一次 Load 的顺序】
  1. 文件名去扩展名，得到模块名；重名拒装。
  2. LoadLibrary / dlopen。
  3. 调 xy_abi_version_v1()；版本不等拒装。
  4. 找 xy_program_v1 / xy_commands_v1；两个都没有拒装。
  5. 有命令表：先登记命令，再按需把微程序包成同名命令。
  6. 全部成功才留下 Module；中途失败回滚命令和服务。

【卸载】
  先摘该 owner 的命令，再摘它名下的服务（有剩的就记泄漏），
  最后销毁 Library。命令表里的指针必须先摘掉，不能悬垂。

【生命周期】
  host / services / commands / currentOwner 都必须比本对象活得久。
  典型持有者是 Engine：它拥有 Services、CommandTable、host 和
  currentModule；main 里的 ModuleRegistry 最后析构。
*/

#include "Platform/Library.h"

#include <xy_api.h>
#include <xy_command.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace X_Y
{
    class CommandTable;
    class Services;

    class ModuleRegistry
    {
    public:
        struct Info
        {
            std::string name;
            std::string path;
            std::string owner;
            bool hasProgram = false;
            std::vector<std::string> commands;
        };

        ModuleRegistry(const XyHostV1& host,
                       Services& services,
                       CommandTable& commands,
                       std::string& currentOwner);
        ~ModuleRegistry();

        ModuleRegistry(const ModuleRegistry&) = delete;
        ModuleRegistry& operator=(const ModuleRegistry&) = delete;

        // 成功返回 true；失败返回 false 并填 error，不留下任何状态。
        bool Load(const std::string& path, std::string& error);

        // 按模块名卸载。leaks 返回该模块名下没反注册的服务数量。
        bool Unload(const std::string& name, int& leaks, std::string& error);

        // 逆序卸载全部；返回发现的泄漏总数。
        int UnloadAll();

        bool IsLoaded(const std::string& name) const;
        std::size_t Count() const { return modules_.size(); }

        // 按装载顺序列出。
        std::vector<Info> List() const;

    private:
        // 微程序包成命令时，invoke 需要知道入口和 host。
        struct ProgramBinding
        {
            xy_program_v1_fn entry = nullptr;
            const XyHostV1* host = nullptr;
            std::string name;
        };

        struct Module
        {
            std::string name;
            std::string path;
            std::string owner;
            Platform::Library library;
            xy_program_v1_fn program = nullptr;
            xy_module_v1_fn moduleInit = nullptr;
            xy_module_shutdown_v1_fn shutdown = nullptr;

            std::unique_ptr<ProgramBinding> binding;
            std::unique_ptr<XyCommandV1> programCommand; // 包出来的那条命令
            std::vector<std::string> commands;           // 含包出来的那条
        };

        static int InvokeProgram(void* self, int argc, const char* const* argv);
        static std::string BaseName(const std::string& path);

        Module* Find(const std::string& name);

        const XyHostV1* host_ = nullptr;
        Services& services_;
        CommandTable& commands_;
        std::string& currentOwner_;
        std::vector<Module> modules_;
    };
}
