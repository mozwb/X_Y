#include "Services.h"

namespace xy
{
    int Services::Add(const std::string& name, void* ptr, const std::string& owner)
    {
        if (name.empty() || !ptr)
        {
            return -1;
        }
        if (entries_.count(name))
        {
            return -2; // 重名：调用方自己换名字，内核不替它决定
        }
        entries_.emplace(name, Entry{ptr, owner});
        return 0;
    }

    int Services::AddBuiltin(const std::string& name, void* ptr)
    {
        return Add(name, ptr, "engine");
    }

    void* Services::Get(const std::string& name) const
    {
        const auto it = entries_.find(name);
        return it == entries_.end() ? nullptr : it->second.ptr;
    }

    void Services::Remove(const std::string& name)
    {
        entries_.erase(name);
    }

    std::vector<std::string> Services::OwnedBy(const std::string& owner) const
    {
        std::vector<std::string> out;
        for (const auto& [name, e] : entries_)
        {
            if (e.owner == owner)
            {
                out.push_back(name);
            }
        }
        return out;
    }

    std::string Services::OwnerOf(const std::string& name) const
    {
        const auto it = entries_.find(name);
        return it == entries_.end() ? std::string() : it->second.owner;
    }

    std::vector<std::string> Services::NamesWithPrefix(const std::string& prefix) const
    {
        std::vector<std::string> out;
        for (const auto& [name, e] : entries_)
        {
            if (name.compare(0, prefix.size(), prefix) == 0)
            {
                out.push_back(name);
            }
        }
        return out;
    }
}
