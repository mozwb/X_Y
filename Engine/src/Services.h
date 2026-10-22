// 服务表：名字 → void*。内核只做存储与查找，不理解语义。
// 记录注册者，卸载模块时才能真正查出「谁忘了反注册」。
#pragma once

#include <map>
#include <string>
#include <vector>

namespace X_Y
{
    class Services
    {
    public:
        // owner 为注册者标识（模块名）；内核自己注册时传 "engine"
        int Add(const std::string& name, void* ptr, const std::string& owner);
        void* Get(const std::string& name) const;
        void Remove(const std::string& name);

        // 该注册者名下的所有服务名（卸载前反查用）
        std::vector<std::string> OwnedBy(const std::string& owner) const;

        // 名字前缀匹配（命令表靠它把 cmd.* 全捞出来）
        std::vector<std::string> NamesWithPrefix(const std::string& prefix) const;

        // 谁提供的这个服务（用于错误信息）
        std::string OwnerOf(const std::string& name) const;

        size_t Size() const { return entries_.size(); }

        // 引擎侧内建服务用：没有 owner 概念，不做泄漏检查
        int AddBuiltin(const std::string& name, void* ptr);

    private:
        struct Entry
        {
            void* ptr = nullptr;
            std::string owner;
        };
        std::map<std::string, Entry> entries_;
    };
}
