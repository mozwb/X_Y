#pragma once
/*
Platform/Library.h —— 动态库的内部封装。

只做三件事：打开、找符号、关闭。
不认 xy_program_v1 / xy_commands_v1，也不做 ABI 检查 —— 那是 ModuleRegistry 的事。

平台差异全部藏在 Platform/Win32/LibraryWin32.cpp 和
Platform/Posix/LibraryPosix.cpp 里；这个头不出现 windows.h / dlfcn.h。

约定：
  . Open 失败时 handle 保持为空，错误写进 error；成功才设置 Path()。
  . Symbol 找不到返回 nullptr，不区分“没有这个符号”和“库没打开”。
  . Close 幂等；析构自动 Close。
  . 可移动、不可拷贝：ModuleRegistry 需要把句柄放进 vector。
*/

#include <string>
#include <utility>

namespace X_Y::Platform
{
    class Library
    {
    public:
        Library() = default;
        ~Library() { Close(); }

        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;

        Library(Library&& other) noexcept
            : handle_(other.handle_)
            , path_(std::move(other.path_))
        {
            other.handle_ = nullptr;
        }

        Library& operator=(Library&& other) noexcept
        {
            if (this != &other)
            {
                Close();
                handle_ = other.handle_;
                path_ = std::move(other.path_);
                other.handle_ = nullptr;
            }
            return *this;
        }

        // path 是 UTF-8。成功返回 true；失败返回 false 并填 error。
        bool Open(const std::string& path, std::string& error);

        // 关闭句柄并清空 path；重复调用无事发生。
        void Close();

        // 找不到返回 nullptr。
        void* Symbol(const char* name) const;

        bool IsOpen() const { return handle_ != nullptr; }
        explicit operator bool() const { return IsOpen(); }

        const std::string& Path() const { return path_; }

    private:
        void* handle_ = nullptr;
        std::string path_;
    };
}
