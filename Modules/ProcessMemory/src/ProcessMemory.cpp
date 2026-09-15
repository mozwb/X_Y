#include "../ProcessMemory.h"
#include "../ProcessMemoryImpl.h"

namespace X_Y
{

    // ✅ 重点：初始化列表直接调用平台工厂创建Impl
    ProcessMemory::ProcessMemory()
        : Impl(ProcessMemoryFactory::CreateProcessMemoryImpl())
    {
    }

    ProcessMemory::~ProcessMemory()
    {
        if (Impl)
        {
            Impl->Detach();
        }
    }

    ProcessMemory::ProcessMemory(ProcessMemory &&other) noexcept
    {
        Impl = std::move(other.Impl);
    }

    ProcessMemory &ProcessMemory::operator=(ProcessMemory &&other) noexcept
    {
        if (this == &other)
            return *this;
        Impl = std::move(other.Impl);
        return *this;
    }

    bool ProcessMemory::Attach(uint32_t pid)
    {
        if (!Impl)
            return false;
        return Impl->Attach(pid);
    }

    bool ProcessMemory::Attach(const std::string &processName)
    {
        if (!Impl)
            return false;
        return Impl->Attach(processName);
    }

    void ProcessMemory::Detach()
    {
        if (Impl)
            Impl->Detach();
    }

    bool ProcessMemory::IsAttached() const
    {
        if (!Impl)
            return false;
        return Impl->IsAttached();
    }

    bool ProcessMemory::ReadRaw(uintptr_t address, Buffer *buffer, size_t bytes)
    {
        if (!Impl)
            return false;
        return Impl->ReadRaw(address, buffer, bytes);
    }

    bool ProcessMemory::WriteRaw(uintptr_t address, const Buffer *buffer, size_t bytes)
    {
        if (!Impl)
            return false;
        return Impl->WriteRaw(address, buffer, bytes);
    }

    uintptr_t ProcessMemory::GetModuleBase(const std::string &moduleName)
    {
        if (!Impl)
            return 0;
        return Impl->GetModuleBase(moduleName);
    }

    uintptr_t ProcessMemory::PatternScan(uintptr_t startAddr, size_t scanRange, const char *pattern, const char *mask)
    {
        if (!Impl)
            return 0;
        return Impl->PatternScan(startAddr, scanRange, pattern, mask);
    }

    std::string ProcessMemory::GetLastErrorString() const
    {
        if (!Impl)
            return "Impl is null";
        return Impl->GetLastErrorString();
    }
}