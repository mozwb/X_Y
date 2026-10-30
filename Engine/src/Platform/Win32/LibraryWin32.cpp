#include "../Library.h"

#include <windows.h>

#include <string>

namespace X_Y::Platform
{
    namespace
    {
        // 路径统一从 UTF-8 进来，LoadLibraryW 要 UTF-16。
        // 用 MB_ERR_INVALID_CHARS 让非法 UTF-8 直接失败，而不是悄悄替换成 '?'。
        std::wstring Utf8ToWide(const std::string& text)
        {
            if (text.empty())
            {
                return {};
            }

            const int length = static_cast<int>(text.size());
            const int wideLength = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
            if (wideLength <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
            const int written = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, wide.data(), wideLength);
            if (written != wideLength)
            {
                return {};
            }
            return wide;
        }
    }

    bool Library::Open(const std::string& path, std::string& error)
    {
        Close();
        error.clear();

        if (path.empty())
        {
            error = "Library::Open: 路径为空";
            return false;
        }

        const std::wstring wide = Utf8ToWide(path);
        if (wide.empty())
        {
            error = "Library::Open: 路径不是合法 UTF-8: " + path;
            return false;
        }

        const HMODULE handle = ::LoadLibraryW(wide.c_str());
        if (handle == nullptr)
        {
            error = "LoadLibraryW 失败，错误码 " +
                    std::to_string(static_cast<unsigned long>(::GetLastError())) +
                    ": " + path;
            return false;
        }

        handle_ = reinterpret_cast<void*>(handle);
        path_ = path;
        return true;
    }

    void Library::Close()
    {
        if (handle_ != nullptr)
        {
            ::FreeLibrary(reinterpret_cast<HMODULE>(handle_));
            handle_ = nullptr;
        }
        path_.clear();
    }

    void* Library::Symbol(const char* name) const
    {
        if (handle_ == nullptr || name == nullptr)
        {
            return nullptr;
        }
        return reinterpret_cast<void*>(
            ::GetProcAddress(reinterpret_cast<HMODULE>(handle_), name));
    }
}
