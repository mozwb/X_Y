// 命令表：从服务表里把 cmd.* 捞出来，做成 名字 → XyCommandV1* 的视图。
// 不自己存状态，每次查之前刷新 —— 模块装卸后自动跟上，不会留旧指针。
#pragma once

#include <xy_command.h>

#include <map>
#include <string>
#include <vector>

namespace xy
{
    class Services;

    class CommandTable
    {
    public:
        void Refresh(const Services& services);

        const XyCommandV1* Find(const std::string& name) const;

        // 全部命令名（按字典序）
        std::vector<std::string> Names() const;

    private:
        std::map<std::string, const XyCommandV1*> commands_;
    };
}
