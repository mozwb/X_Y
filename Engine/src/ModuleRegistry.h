// 模块注册表：负责 装载 → 初始化 → （逆序）反注册 → 卸载。
// 头号风险是「模块卸载后 host 里还留着它的指针」，所以卸载流程把检查做实。
#pragma once

#include <xy_api.h>

#include <string>
#include <vector>

namespace X_Y
{
    struct EngineContext;

    class ModuleRegistry
    {
    public:
        explicit ModuleRegistry(EngineContext& ctx) : ctx_(ctx) {}
        ~ModuleRegistry() { UnloadAll(); }

        ModuleRegistry(const ModuleRegistry&) = delete;
        ModuleRegistry& operator=(const ModuleRegistry&) = delete;

        // 装载并初始化一个模块 DLL。失败时不留下任何状态。
        bool Load(const std::string& path);

        // 逆序卸载所有模块；返回卸载期间发现的泄漏数（0 表示干净）
        int UnloadAll();

        size_t Count() const { return modules_.size(); }

    private:
        struct Module
        {
            std::string path;
            std::string name;  // 显示用（文件名）
            std::string owner; // 归属键：name#序号，保证同名 dll 不串味
            void* handle = nullptr;
            xy_module_shutdown_v1_fn shutdown = nullptr;
        };

        bool Unload(Module& m, int& leaks);
        static std::string BaseName(const std::string& path, bool stripExt);

        EngineContext& ctx_;
        std::vector<Module> modules_;
        unsigned seq_ = 0;
    };
}
