#pragma once
#include "Buffer/include/Buffer.h"
#include "FilesSystem/include/FilesSystem.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace X_Y {

class DataStore {
public:
    static DataStore& Instance();

    // ── 数据库操作 ──
    void   Insert(const std::string& key, Buffer data);
    void   Append(const std::string& key, const Buffer& data);
    Buffer* Get(const std::string& key);
    const Buffer* Get(const std::string& key) const;
    bool   Remove(const std::string& key);
    void   Rename(const std::string& oldKey, const std::string& newKey);
    bool   Contains(const std::string& key) const;
    std::vector<std::string> ListKeys() const;

    // ── 持久化 ──
    void Flush(const std::string& key);
    void FlushAll();
    bool LoadFile(const std::string& filepath);
    void LoadDirectory(const XPath& dir);

    // ── 配置 ──
    void SetDataDir(const std::string& dir);
    const XPath& GetDataDir() const { return m_DataDir; }

private:
    DataStore() = default;
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    // 内部工具：key → 文件路径
    XPath KeyToPath(const std::string& key) const;

    // 内部：从文件读入内存（不经过 Insert，直接设值）
    bool LoadFileInto(const std::string& key, const XPath& path);

    std::unordered_map<std::string, Buffer> m_Entries;
    XPath m_DataDir{"DataStore/"};
};

} 
// namespace X_Y
