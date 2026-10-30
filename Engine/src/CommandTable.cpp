#include "CommandTable.h"

namespace X_Y
{
    bool CommandTable::Add(const XyCommandV1* cmd, const std::string& owner)
    {
        if (cmd == nullptr || cmd->size < sizeof(XyCommandV1) ||
            cmd->name == nullptr || cmd->name[0] == '\0' ||
            cmd->invoke == nullptr || owner.empty())
        {
            return false;
        }

        // 重名不覆盖；emplace 正好是这个语义。
        const auto [it, inserted] = commands_.emplace(cmd->name, Entry{cmd, owner});
        (void)it;
        return inserted;
    }

    const XyCommandV1* CommandTable::Find(const std::string& name) const
    {
        const auto it = commands_.find(name);
        return it == commands_.end() ? nullptr : it->second.cmd;
    }

    std::string CommandTable::OwnerOf(const std::string& name) const
    {
        const auto it = commands_.find(name);
        return it == commands_.end() ? std::string{} : it->second.owner;
    }

    bool CommandTable::Remove(const std::string& name)
    {
        return commands_.erase(name) != 0;
    }

    std::size_t CommandTable::RemoveByOwner(const std::string& owner)
    {
        std::size_t removed = 0;
        for (auto it = commands_.begin(); it != commands_.end();)
        {
            if (it->second.owner == owner)
            {
                it = commands_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        return removed;
    }

    std::vector<std::string> CommandTable::Names() const
    {
        std::vector<std::string> out;
        out.reserve(commands_.size());
        for (const auto& item : commands_)
        {
            out.push_back(item.first);
        }
        return out;
    }
}
