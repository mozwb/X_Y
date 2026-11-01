#include <xy_api.h>
#include <xy_kernel.h>

#include <Terminal/Shell.h>
#include <Terminal/Command.h>

#include <cstdio>
#include <string>
#include <vector>

namespace
{
    X_Y::Command::Result RunThroughKernel(const XyKernelV1* kernel,
                                          const X_Y::Command::Args& args)
    {
        std::vector<const char*> argv;
        argv.reserve(args.Items.size() + 1);
        argv.push_back(args.Name.c_str());
        for (const std::string& item : args.Items)
        {
            argv.push_back(item.c_str());
        }

        const int code = kernel->run(
            kernel->user, static_cast<int>(argv.size()), argv.data());

        X_Y::Command::Result result;
        result.ExitCode = code;
        result.Started = true;
        if (code == 127)
        {
            result.Error = "没有命令: " + args.Name;
        }
        return result;
    }
}

XY_EXPORT uint32_t xy_abi_version_v1(void)
{
    return XY_ABI_VERSION;
}

XY_EXPORT int xy_program_v1(const XyHostV1* host, int argc, const char* const* argv)
{
    (void)argc;
    (void)argv;

    if (host == nullptr)
    {
        return 3;
    }

    const XyKernelV1* kernel =
        (const XyKernelV1*)host->get_service(host->user, XY_KERNEL_SERVICE);
    if (kernel == nullptr ||
        kernel->size < sizeof(XyKernelV1) ||
        kernel->version != XY_KERNEL_VERSION)
    {
        std::fprintf(stderr, "Shell: kernel service not found\n");
        return 3;
    }

    X_Y::Command::Registry registry;
    registry.SetExternalEnabled(false);
    registry.SetFallback([kernel](const X_Y::Command::Args& args)
    {
        return RunThroughKernel(kernel, args);
    });

    X_Y::Shell::Shell shell(registry);
    shell.SetPrompt("xy> ");
    return shell.Run();
}
