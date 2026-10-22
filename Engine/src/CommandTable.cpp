#include "CommandTable.h"

#include "Services.h"

namespace X_Y
{
    void CommandTable::Refresh(const Services& services)
    {
        commands_.clear();
        for (const auto& full : services.NamesWithPrefix(XY_CMD_SERVICE_PREFIX))
        {
            auto* cmd = static_cast<const XyCommandV1*>(services.Get(full));
            if (!cmd || !cmd->name || !cmd->invoke)
            {
                continue;
            }
            // 用命令自己声明的 name，而不是服务名后缀：
            // 这样模块换个服务名也不会悄悄改掉用户要敲的名字。
            commands_[cmd->name] = cmd;
        }
    }

    const XyCommandV1* CommandTable::Find(const std::string& name) const
    {
        const auto it = commands_.find(name);
        return it == commands_.end() ? nullptr : it->second;
    }

    std::vector<std::string> CommandTable::Names() const
    {
        std::vector<std::string> out;
        out.reserve(commands_.size());
        for (const auto& [name, cmd] : commands_)
        {
            out.push_back(name);
        }
        return out;
    }
}
