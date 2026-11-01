#include "Host.h"

#include "EngineContainer.h"
#include "KernelCommands.h"

#include <Memory/Memory.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

namespace X_Y
{
    namespace
    {
        // Memory 只保证 max_align；更大的我们暂时不支持，直接返回 NULL，
        // 不在 host 里再混一套 aligned 分配。
        constexpr std::size_t kMaxAlign = alignof(std::max_align_t);

        const char* LevelTag(int level)
        {
            switch (level)
            {
            case XY_LOG_TRACE: return "trace";
            case XY_LOG_INFO:  return "info";
            case XY_LOG_WARN:  return "warn";
            case XY_LOG_ERROR: return "error";
            default:           return "?";
            }
        }

        void HostLog(void*, int level, const char* message)
        {
            std::fprintf(stderr, "[%s] %s\n",
                         LevelTag(level),
                         message != nullptr ? message : "(null)");
            std::fflush(stderr);
        }

        void* HostAlloc(void*, std::size_t size, std::size_t align)
        {
            if (size == 0)
            {
                return nullptr;
            }

            if (align != 0)
            {
                const bool powerOfTwo = (align & (align - 1)) == 0;
                if (!powerOfTwo || align > kMaxAlign)
                {
                    return nullptr;
                }
            }

            return Memory::Instance().allocate(size);
        }

        void HostFree(void*, void* ptr)
        {
            if (ptr != nullptr)
            {
                Memory::Instance().deallocate(ptr);
            }
        }

        int HostAddService(void* user, const char* name, void* ptr)
        {
            auto* ctx = static_cast<Engine*>(user);
            if (ctx == nullptr || name == nullptr || ptr == nullptr)
            {
                return -1;
            }

            // 一次性程序返回后 DLL 就卸了，它发布的指针活不过调用；
            // 在源头拒绝，比事后摘表干净。
            if (ctx->oneShot)
            {
                std::fprintf(stderr,
                             "X_Y: 一次性程序 %s 不能注册服务 '%s'\n",
                             ctx->currentModule.c_str(), name);
                return -1;
            }

            // 归属由引擎标注，模块无法伪装。
            const std::string owner =
                ctx->currentModule.empty() ? std::string("engine") : ctx->currentModule;
            return ctx->services.Add(name, ptr, owner) ? 0 : 1;
        }

        void* HostGetService(void* user, const char* name)
        {
            auto* ctx = static_cast<Engine*>(user);
            if (ctx == nullptr || name == nullptr)
            {
                return nullptr;
            }

            // 先本地，再容器共享表；子 engine 最后再看 root。
            if (void* local = ctx->services.Get(name); local != nullptr)
            {
                return local;
            }
            if (ctx->container != nullptr)
            {
                if (void* shared = ctx->container->GetShared(name); shared != nullptr)
                {
                    return shared;
                }

                Engine& root = ctx->container->Root();
                if (&root != ctx)
                {
                    return root.services.Get(name);
                }
            }
            return nullptr;
        }

        void HostRemoveService(void* user, const char* name)
        {
            auto* ctx = static_cast<Engine*>(user);
            if (ctx == nullptr || name == nullptr)
            {
                return;
            }

            // 注册时引擎标了归属，删除时必须验；currentModule 为空 = 引擎自己在删。
            if (!ctx->currentModule.empty())
            {
                const std::string owner = ctx->services.OwnerOf(name);
                if (!owner.empty() && owner != ctx->currentModule)
                {
                    std::fprintf(stderr,
                                 "X_Y: 拒绝 %s 删除他人服务 '%s'（归属 %s）\n",
                                 ctx->currentModule.c_str(), name, owner.c_str());
                    return;
                }
            }

            ctx->services.Remove(name);
        }

        int KernelRun(void* user, int argc, const char* const* argv)
        {
            auto* engine = static_cast<Engine*>(user);
            if (engine == nullptr || argc < 1 || argv == nullptr || argv[0] == nullptr)
            {
                return 2;
            }
            return RunCommand(*engine, argc, argv);
        }
    }

    void Engine::Start(bool withBuiltins)
    {
        BuildHost(*this);
        if (withBuiltins)
        {
            Memory::Instance().markBaseline();
        }
        modules = std::make_unique<ModuleRegistry>(
            host, services, commands, currentModule);
        if (withBuiltins)
        {
            RegisterKernelCommands(*this);
        }

        kernel.size = sizeof(XyKernelV1);
        kernel.version = XY_KERNEL_VERSION;
        kernel.user = this;
        kernel.run = &KernelRun;
        if (!services.Add(XY_KERNEL_SERVICE, &kernel, "engine"))
        {
            std::fprintf(stderr, "X_Y: 注册 %s 服务失败\n", XY_KERNEL_SERVICE);
        }
    }

    const XyHostV1* BuildHost(Engine& ctx)
    {
        // host->alloc 的契约是失败返回 NULL，不能因为一次大分配 terminate 进程。
        Memory::Instance().setOOMAction(OOMAction::ReturnNull);

        ctx.host.size = sizeof(XyHostV1);
        ctx.host.abi_version = XY_ABI_VERSION;
        ctx.host.user = &ctx;

        ctx.host.alloc = &HostAlloc;
        ctx.host.free = &HostFree;
        ctx.host.log = &HostLog;

        ctx.host.add_service = &HostAddService;
        ctx.host.get_service = &HostGetService;
        ctx.host.remove_service = &HostRemoveService;

        return &ctx.host;
    }

    int RunCommand(Engine& engine, int argc, const char* const* argv)
    {
        if (argc < 1 || argv == nullptr || argv[0] == nullptr)
        {
            return 2;
        }

        const CommandRef ref = FindCommand(engine, argv[0]);
        if (ref.command == nullptr)
        {
            std::fprintf(stderr, "没有命令 '%s'\n", argv[0]);
            return 127;
        }

        struct OwnerScope
        {
            std::string& slot;
            std::string previous;

            OwnerScope(std::string& target, std::string owner)
                : slot(target), previous(std::move(target))
            {
                slot = std::move(owner);
            }

            ~OwnerScope()
            {
                slot = std::move(previous);
            }
        };

        OwnerScope scope(engine.currentModule, ref.owner);
        return ref.command->invoke(ref.command->self, argc, argv);
    }

    CommandRef FindCommand(Engine& engine, const std::string& name)
    {
        if (const XyCommandV1* command = engine.commands.Find(name))
        {
            return {command, engine.commands.OwnerOf(name)};
        }

        if (engine.container != nullptr)
        {
            if (const XyCommandV1* command = engine.container->GetSharedCommand(name))
            {
                return {command, engine.container->SharedCommandOwner(name)};
            }

            Engine& root = engine.container->Root();
            if (&root != &engine)
            {
                if (const XyCommandV1* command = root.commands.Find(name))
                {
                    return {command, root.commands.OwnerOf(name)};
                }
            }
        }

        return {};
    }
}
