#include "Services.h"

namespace X_Y
{
    bool Services::Add(const std::string& name, void* ptr, const std::string& owner)
    {
        if (name.empty() || ptr == nullptr || owner.empty())
        {
            return false;
        }

        // emplace 在重名时不会覆盖旧值，正好是我们要的语义。
        const auto [it, inserted] = entries_.emplace(name, Entry{ptr, owner});
        (void)it;
        return inserted;
    }

    void* Services::Get(const std::string& name) const
    {
        const auto it = entries_.find(name);
        return it == entries_.end() ? nullptr : it->second.ptr;
    }

    bool Services::Remove(const std::string& name)
    {
        return entries_.erase(name) != 0;
    }

    std::string Services::OwnerOf(const std::string& name) const
    {
        const auto it = entries_.find(name);
        return it == entries_.end() ? std::string{} : it->second.owner;
    }

    std::vector<std::string> Services::OwnedBy(const std::string& owner) const
    {
        std::vector<std::string> out;
        for (const auto& [name, entry] : entries_)
        {
            if (entry.owner == owner)
            {
                out.push_back(name);
            }
        }
        return out;
    }

    std::vector<std::string> Services::Names() const
    {
        std::vector<std::string> out;
        out.reserve(entries_.size());
        for (const auto& item : entries_)
        {
            out.push_back(item.first);
        }
        return out;
    }
}
