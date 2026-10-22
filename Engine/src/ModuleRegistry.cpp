#include "ModuleRegistry.h"

#include "Host.h"

#include <cstdio>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace xy
{
#if defined(_WIN32)
    using NativeHandle = ::HMODULE;
#else
    using NativeHandle = void*;
#endif

    namespace
    {
        // Module::handle 在头文件里是 void*，避免把平台类型漏出去
        NativeHandle AsHandle(void* h) { return reinterpret_cast<NativeHandle>(h); }

        NativeHandle OpenLibrary(const std::string& path)
        {
#if defined(_WIN32)
            return ::LoadLibraryA(path.c_str());
#else
            return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        }

        void CloseLibrary(NativeHandle h)
        {
#if defined(_WIN32)
            ::FreeLibrary(h);
#else
            ::dlclose(h);
#endif
        }

        void* FindSymbol(NativeHandle h, const char* name)
        {
#if defined(_WIN32)
            return reinterpret_cast<void*>(::GetProcAddress(h, name));
#else
            return ::dlsym(h, name);
#endif
        }
    }

    std::string ModuleRegistry::BaseName(const std::string& path, bool stripExt)
    {
        const size_t slash = path.find_last_of("/\\");
        std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        if (stripExt)
        {
            const size_t dot = name.find_last_of('.');
            if (dot != std::string::npos)
            {
                name.erase(dot);
            }
        }
        return name;
    }

    bool ModuleRegistry::Load(const std::string& path)
    {
        Module m;
        m.path = path;
        m.name = BaseName(path, true);
        // 归属键带序号：不同目录下的同名 dll 不会互相冒领服务
        m.owner = m.name + "#" + std::to_string(++seq_);

        m.handle = OpenLibrary(path);
        if (!m.handle)
        {
            std::fprintf(stderr, "xy_engine: 载入模块 %s 失败\n", path.c_str());
            return false;
        }

        auto init = reinterpret_cast<xy_module_v1_fn>(
            FindSymbol(AsHandle(m.handle), XY_MODULE_ENTRY));
        if (!init)
        {
            std::fprintf(stderr, "xy_engine: %s 没有导出 %s\n",
                         m.name.c_str(), XY_MODULE_ENTRY);
            CloseLibrary(AsHandle(m.handle));
            return false;
        }

        // shutdown 是可选的：不给就必须自己保证不往 host 里留东西
        m.shutdown = reinterpret_cast<xy_module_shutdown_v1_fn>(
            FindSymbol(AsHandle(m.handle), XY_MODULE_SHUTDOWN_ENTRY));

        // 归属由引擎标注，模块无法伪装
        ctx_.currentModule = m.owner;
        const int rc = init(&ctx_.host);
        ctx_.currentModule.clear();

        if (rc != 0)
        {
            std::fprintf(stderr, "xy_engine: 模块 %s 初始化失败 rc=%d\n",
                         m.name.c_str(), rc);
            // 初始化失败也可能已经注册了一半，按卸载流程回滚
            int leaks = 0;
            Module rollback = m;
            Unload(rollback, leaks);
            return false;
        }

        modules_.push_back(m);
        return true;
    }

    bool ModuleRegistry::Unload(Module& m, int& leaks)
    {
        // 1. 先让模块自己反注册
        ctx_.currentModule = m.owner;
        if (m.shutdown)
        {
            m.shutdown();
        }
        else
        {
            std::fprintf(stderr,
                         "xy_engine: 警告 %s 未导出 %s，只能靠引擎代查泄漏\n",
                         m.name.c_str(), XY_MODULE_SHUTDOWN_ENTRY);
        }
        ctx_.currentModule.clear();

        // 2. 反查它名下还剩下的服务 —— 这些指针马上就要悬垂了
        const auto left = ctx_.services.OwnedBy(m.owner);
        for (const auto& s : left)
        {
            std::fprintf(stderr,
                         "xy_engine: 泄漏 %s 注册的服务 '%s'（未反注册，已强制移除）\n",
                         m.name.c_str(), s.c_str());
            ctx_.services.Remove(s);
            ++leaks;
        }

        // 3. 真正卸载
        if (m.handle)
        {
            CloseLibrary(AsHandle(m.handle));
            m.handle = nullptr;
        }
        return true;
    }

    int ModuleRegistry::UnloadAll()
    {
        int leaks = 0;
        // 逆序：后装载的先卸，和构造/析构顺序一致
        for (auto it = modules_.rbegin(); it != modules_.rend(); ++it)
        {
            Unload(*it, leaks);
        }
        modules_.clear();
        return leaks;
    }
}
