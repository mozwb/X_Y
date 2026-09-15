#include "../Win32/ProcessMemoryWin32Impl.h"
#include <TlHelp32.h>
#include <cstring>
#include <sstream>
namespace X_Y
{
    ProcessMemoryWin32Impl::ProcessMemoryWin32Impl()
    {
    }
    ProcessMemoryWin32Impl::~ProcessMemoryWin32Impl()
    {
        Detach();
    }
    bool ProcessMemoryWin32Impl::Attach(uint32_t pid)
    {
        Detach();
        // 打开进程，获取读写内存+查询信息权限
        m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        m_lastError = GetLastError();
        return m_hProcess != nullptr;
    }
    bool ProcessMemoryWin32Impl::Attach(const std::string &processName)
    {
        Detach();
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
        {
            m_lastError = GetLastError();
            return false;
        }

        PROCESSENTRY32W pe32{}; // 改成宽字符版本 PROCESSENTRY32W
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (!Process32FirstW(hSnapshot, &pe32)) // 宽字符函数 Process32FirstW
        {
            m_lastError = GetLastError();
            CloseHandle(hSnapshot);
            return false;
        }

        // std::string -> std::wstring
        std::wstring processNameW(processName.begin(), processName.end());

        bool found = false;
        do
        {
            // 宽字符忽略大小写比较 _wcsicmp
            if (_wcsicmp(pe32.szExeFile, processNameW.c_str()) == 0)
            {
                m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
                m_lastError = GetLastError();
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32)); // Process32NextW

        CloseHandle(hSnapshot);
        if (!found || m_hProcess == nullptr)
            return false;
        return true;
    }
    void ProcessMemoryWin32Impl::Detach()
    {
        if (m_hProcess != nullptr)
        {
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
    }

    bool ProcessMemoryWin32Impl::IsAttached() const
    {
        return m_hProcess != nullptr;
    }
    // ========== ReadRaw：读取远程内存写入X_Y::Buffer ==========
    bool ProcessMemoryWin32Impl::ReadRaw(uintptr_t address, X_Y::Buffer *buffer, size_t bytes)
    {
        if (!IsAttached() || buffer == nullptr || bytes == 0)
            return false;

        // 自动扩容，保证buffer有足够空间
        if (!buffer->Ensure(bytes))
            return false;

        SIZE_T readBytes = 0;
        BOOL ok = ReadProcessMemory(
            m_hProcess,
            reinterpret_cast<LPCVOID>(address),
            buffer->Data,
            bytes,
            &readBytes);
        m_lastError = GetLastError();

        // 全部字节读取成功
        if (ok && readBytes == bytes)
        {
            buffer->Size = bytes; // 更新Buffer有效数据长度
            return true;
        }
        return false;
    }

    // ========== WriteRaw：从X_Y::Buffer读取数据写入远程进程 ==========
    bool ProcessMemoryWin32Impl::WriteRaw(uintptr_t address, const X_Y::Buffer *buffer, size_t bytes)
    {
        if (!IsAttached() || buffer == nullptr || bytes == 0 || buffer->Data == nullptr)
            return false;

        // 不能超过Buffer内部有效数据大小
        const size_t actualWriteSize = std::min<size_t>(bytes, static_cast<size_t>(buffer->Size));
        if (actualWriteSize == 0)
            return false;

        SIZE_T writtenBytes = 0;
        BOOL ok = WriteProcessMemory(
            m_hProcess,
            reinterpret_cast<LPVOID>(address),
            buffer->Data,
            actualWriteSize,
            &writtenBytes);
        m_lastError = GetLastError();

        return ok && writtenBytes == actualWriteSize;
    }

    uintptr_t ProcessMemoryWin32Impl::GetModuleBase(const std::string &moduleName)
    {
        if (!IsAttached())
            return 0;

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetProcessId(m_hProcess));
        if (hSnapshot == INVALID_HANDLE_VALUE)
        {
            m_lastError = GetLastError();
            return 0;
        }

        MODULEENTRY32W me32{};
        me32.dwSize = sizeof(MODULEENTRY32W);
        if (!Module32FirstW(hSnapshot, &me32))
        {
            m_lastError = GetLastError();
            CloseHandle(hSnapshot);
            return 0;
        }

        std::wstring moduleNameW(moduleName.begin(), moduleName.end());
        uintptr_t baseAddr = 0;
        do
        {
            if (_wcsicmp(me32.szModule, moduleNameW.c_str()) == 0)
            {
                baseAddr = reinterpret_cast<uintptr_t>(me32.modBaseAddr);
                break;
            }
        } while (Module32NextW(hSnapshot, &me32));

        CloseHandle(hSnapshot);
        return baseAddr;
    }

    uintptr_t ProcessMemoryWin32Impl::PatternScan(uintptr_t startAddr, size_t scanRange, const char *pattern, const char *mask)
    {
        if (!IsAttached() || scanRange == 0 || pattern == nullptr || mask == nullptr)
            return 0;

        size_t patternLen = strlen(mask);
        if (patternLen > scanRange)
            return 0;

        // 使用Buffer作为扫描缓冲区，复用你写好的Buffer + Memory分配器
        X_Y::Buffer scanBuf;
        if (!scanBuf.Reserve(scanRange))
            return 0;
        scanBuf.Size = scanRange; // 预留空间，标记有效长度，供ReadRaw写入

        // 调用我们刚写好的ReadRaw，把远程内存读到scanBuf
        bool readSuccess = ReadRaw(startAddr, &scanBuf, scanRange);
        if (!readSuccess)
        {
            scanBuf.Release();
            return 0;
        }

        uintptr_t result = 0;
        for (size_t i = 0; i <= scanRange - patternLen; i++)
        {
            bool matched = true;
            for (size_t j = 0; j < patternLen; j++)
            {
                if (mask[j] == '?')
                    continue;
                if (scanBuf.Data[i + j] != static_cast<uint8_t>(pattern[j]))
                {
                    matched = false;
                    break;
                }
            }
            if (matched)
            {
                result = startAddr + i;
                break;
            }
        }

        scanBuf.Release();
        return result;
    }
    std::string ProcessMemoryWin32Impl::GetLastErrorString() const
    {
        std::ostringstream oss;
        oss << m_lastError;
        return oss.str();
    }
} // namespace X_Y
