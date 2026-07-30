#pragma once
#include "Memory/include/Buffer.h"
#include "FilesSystem/include/FilesSystem.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

namespace X_Y {

// ── 统计 ──
struct DataStoreStats {
    uint64_t EntryCount = 0;
    uint64_t TotalBytes = 0;
    uint64_t TotalCapacity = 0;

    struct EntryInfo {
        std::string Key;
        uint64_t Size;
        uint64_t Capacity;
    };
    std::vector<EntryInfo> Entries;
};

// ── DataStore ──
// 基于文件的 key-value 存储
//
// 规则：
//   - 所有条目由 DS 通过 GetOrCreate 创建，外部不挂载 Buffer
//   - key = 文件路径（含后缀），如 "log"、"assets/tex/brick.img"
//   - 保存时按 key 创建目录，文件内容 = Buffer 数据
//   - .dsidx 索引文件记录所有 key 列表，启动时加载
//   - Save = 覆盖写入，Flush = 追加写入
//   - 析构不自动 Flush/Save，由业务决定

class DataStore {
public:
    static DataStore& Instance();

    // ── 核心操作 ──
    Buffer* GetOrCreate(const std::string& key, uint64_t reserveSize = 4096);
    Buffer* Get(const std::string& key);
    bool    Remove(const std::string& key);
    void    Rename(const std::string& oldKey, const std::string& newKey);
    bool    Contains(const std::string& key) const;
    std::vector<std::string> ListKeys() const;

    // ── 生命周期 ──
    void ClearAll();

    // ── 持久化 ──
    bool Save(const std::string& key);
    bool SaveAll();
    void Flush(const std::string& key);
    void FlushAll();

    bool LoadFile(const std::string& filepath);
    void LoadDirectory(const XPath& dir);

    // ── 配置 ──
    void SetDataDir(const std::string& dir);
    const XPath& GetDataDir() const { return m_DataDir; }

    // ── 统计 ──
    DataStoreStats GetStats() const;
    std::string ToString() const;

private:
    DataStore() = default;
    ~DataStore() = default;
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    XPath KeyToPath(const std::string& key) const;
    XPath IndexPath() const;
    void  EnsureDataDir();
    void  SaveIndex();
    void  LoadIndex();

    std::unordered_map<std::string, Buffer> m_Entries;
    XPath m_DataDir{"DataStore/"};

    mutable std::shared_mutex m_Mutex;
};

} // namespace X_Y
