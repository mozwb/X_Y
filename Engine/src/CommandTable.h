#pragma once
/*
CommandTable.h —— 内核的命令字典：命令名 -> XyCommandV1* + 归属。

它和 Services 不是一回事：
  - Services 存任意 void*，给模块之间 get_service 用
  - CommandTable 只存命令，给 run 分发用

【硬规矩】
  1. 表只存指针，不拥有命令。命令对象由 DLL 或内核自己持有。
  2. 指针必须活到 Remove / RemoveByOwner；卸载前必须先摘表。
  3. 重名拒绝，不覆盖。名字为空、invoke 为空、owner 为空也拒绝。
  4. 名字大小写敏感；建议全小写。
  5. 这张表不执行命令，只负责“找得到、列得出、卸得掉”。
  6. v1 单线程，不加锁。
*/

#include <xy_command.h>

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace X_Y
{
    class CommandTable
    {
    public:
        // 成功返回 true；非法命令 / 重名 / owner 为空返回 false。
        bool Add(const XyCommandV1* cmd, const std::string& owner);

        // 找不到返回 nullptr。
        const XyCommandV1* Find(const std::string& name) const;

        // 找不到返回空串。
        std::string OwnerOf(const std::string& name) const;

        // 返回是否真的删掉了。
        bool Remove(const std::string& name);

        // 摘掉某个 owner 名下的全部命令；返回摘掉的数量。
        std::size_t RemoveByOwner(const std::string& owner);

        // 全部命令名（按字典序）。
        std::vector<std::string> Names() const;

        std::size_t Size() const { return commands_.size(); }

        void Clear() { commands_.clear(); }

    private:
        struct Entry
        {
            const XyCommandV1* cmd = nullptr;
            std::string owner;
        };

        std::map<std::string, Entry> commands_;
    };
}
