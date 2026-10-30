#include "../Library.h"

#include <dlfcn.h>

#include <string>

namespace X_Y::Platform
{
    bool Library::Open(const std::string& path, std::string& error)
    {
        Close();
        error.clear();

        if (path.empty())
        {
            error = "Library::Open: 路径为空";
            return false;
        }

        // dlerror 是“上次错误”的线程局部状态；打开前清一次，
        // 失败时才能确定拿到的是这次的错误。
        ::dlerror();

        void* handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr)
        {
            const char* reason = ::dlerror();
            error = "dlopen 失败: ";
            error += reason != nullptr ? reason : "未知错误";
            error += ": ";
            error += path;
            return false;
        }

        handle_ = handle;
        path_ = path;
        return true;
    }

    void Library::Close()
    {
        if (handle_ != nullptr)
        {
            ::dlclose(handle_);
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

        ::dlerror();
        return ::dlsym(handle_, name);
    }
}
