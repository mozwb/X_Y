// 构建骨架用的临时 main：只证明 X_Y.exe 能编、Memory 能整库链进来。
// 真正的内核 main 会在 Shell / KernelCommands 之后替换掉它。
#include <Memory/Memory.h>
#include <xy_api.h>

#include <cstdio>

int main()
{
    auto& memory = X_Y::Memory::Instance();

    void* block = memory.allocate(64);
    std::printf("X_Y skeleton\n");
    std::printf("  abi    = %u\n", static_cast<unsigned>(XY_ABI_VERSION));
    std::printf("  host   = %zu bytes\n", sizeof(XyHostV1));
    std::printf("  block  = %p\n", block);
    std::printf("  used   = %llu bytes\n",
                static_cast<unsigned long long>(memory.UsedBytes()));

    memory.deallocate(block);
    std::printf("  after  = %llu bytes\n",
                static_cast<unsigned long long>(memory.UsedBytes()));
    return 0;
}
