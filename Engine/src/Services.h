#pragma once
/*
Services.h —— 内核内部的服务表：名字 -> 指针 + 归属。

这张表不是 ABI。DLL 只能通过 host 的 add_service / get_service /
remove_service 摸到它，拿不到 Services 类本身。

【用途】
  模块之间通信的唯一机制。内核只存 名字 -> void*，不理解指针指向什么。
  owner 由内核标注（"engine" / "pkg#1"），模块无法伪装；
  卸载时按 owner 反查，能查出谁忘了反注册。

【硬规矩】
  1. 服务指针归注册方所有；表只借，不释放。
  2. 指针必须活到 remove / 卸载；表里不留悬垂指针是注册方的责任。
  3. 重名拒绝，不覆盖。名字为空、指针为空、owner 为空也拒绝。
  4. v1 单线程；登记和查找都归主线程，不加锁。
*/

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace X_Y
{
    class Services
    {
    public:
        // 成功返回 true；名字为空 / 指针为空 / owner 为空 / 重名返回 false。
        bool Add(const std::string& name, void* ptr, const std::string& owner);

        // 找不到返回 nullptr。
        void* Get(const std::string& name) const;

        // 返回是否真的删掉了。
        bool Remove(const std::string& name);

        // 找不到返回空串。
        std::string OwnerOf(const std::string& name) const;

        // 该 owner 名下的所有服务名（按字典序）。
        std::vector<std::string> OwnedBy(const std::string& owner) const;

        // 全部服务名（按字典序）。
        std::vector<std::string> Names() const;

        std::size_t Size() const { return entries_.size(); }

        void Clear() { entries_.clear(); }

    private:
        struct Entry
        {
            void* ptr = nullptr;
            std::string owner;
        };

        std::map<std::string, Entry> entries_;
    };
}
