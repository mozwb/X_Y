#include "DataStore.h"
#include <cassert>
#include <sstream>
#include <fstream>

namespace X_Y {

// ═════════════════════════════════════════════════════════════════════════════
//  单例
// ═════════════════════════════════════════════════════════════════════════════

DataStore& DataStore::Instance()
{
    static DataStore inst;
    return inst;
}

// ═════════════════════════════════════════════════════════════════════════════
//  内部工具
// ═════════════════════════════════════════════════════════════════════════════

XPath DataStore::KeyToPath(const std::string& key) const
{
    return m_DataDir / key;
}

XPath DataStore::IndexPath() const
{
    return m_DataDir / ".dsidx";
}

void DataStore::EnsureDataDir()
{
    if (!m_DataDir.Exists())
        m_DataDir.CreateDirectory();
}

// ═════════════════════════════════════════════════════════════════════════════
//  索引管理
// ═════════════════════════════════════════════════════════════════════════════

void DataStore::SaveIndex()
{
    EnsureDataDir();

    std::ofstream ofs(IndexPath().Path().string(), std::ios::binary);
    if (!ofs)
        return;

    for (const auto& pair : m_Entries)
        ofs << pair.first << "\n";

    ofs.close();
}

void DataStore::LoadIndex()
{
    XPath idxPath = IndexPath();
    if (!idxPath.Exists())
        return;

    std::ifstream ifs(idxPath.Path().string());
    if (!ifs)
        return;

    std::string line;
    while (std::getline(ifs, line))
    {
        if (!line.empty() && line[0] != '#')
        {
            // 只注册 key，不加载数据（空 Buffer 占位）
            if (m_Entries.find(line) == m_Entries.end())
                m_Entries[line] = Buffer();
        }
    }

    ifs.close();
}

// ═════════════════════════════════════════════════════════════════════════════
//  配置
// ═════════════════════════════════════════════════════════════════════════════

void DataStore::SetDataDir(const std::string& dir)
{
    m_DataDir = dir;
}

// ═════════════════════════════════════════════════════════════════════════════
//  核心操作
// ═════════════════════════════════════════════════════════════════════════════

Buffer* DataStore::GetOrCreate(const std::string& key, uint64_t reserveSize)
{
    std::unique_lock lock(m_Mutex);

    auto it = m_Entries.find(key);
    if (it != m_Entries.end())
    {
        // key 已注册但数据未加载（空 Buffer 占位）
        if (!it->second.Data)
        {
            XPath path = KeyToPath(key);
            if (path.Exists())
            {
                Buffer loaded = FilesSystem::ReadFileBinary(path);
                if (loaded)
                    it->second = std::move(loaded);
            }
        }
        return &it->second;
    }

    // 创建新条目
    Buffer buf(reserveSize);
    auto result = m_Entries.emplace(key, std::move(buf));
    SaveIndex();
    return &result.first->second;
}

Buffer* DataStore::Get(const std::string& key)
{
    std::shared_lock lock(m_Mutex);

    auto it = m_Entries.find(key);
    if (it == m_Entries.end())
        return nullptr;

    // 空 Buffer 占位 → 从文件加载
    if (!it->second.Data)
    {
        lock.unlock();
        std::unique_lock ulock(m_Mutex);

        // 二次检查（防止另一个线程已加载）
        if (!it->second.Data)
        {
            XPath path = KeyToPath(key);
            if (path.Exists())
            {
                Buffer loaded = FilesSystem::ReadFileBinary(path);
                if (loaded)
                    it->second = std::move(loaded);
            }
        }
        return &it->second;
    }

    return &it->second;
}

bool DataStore::Remove(const std::string& key)
{
    std::unique_lock lock(m_Mutex);

    XPath path = KeyToPath(key);
    if (path.Exists())
        path.Remove();

    bool erased = m_Entries.erase(key) > 0;
    if (erased)
        SaveIndex();

    return erased;
}

void DataStore::Rename(const std::string& oldKey, const std::string& newKey)
{
    std::unique_lock lock(m_Mutex);

    auto it = m_Entries.find(oldKey);
    if (it == m_Entries.end())
        return;

    m_Entries[newKey] = std::move(it->second);
    m_Entries.erase(it);

    XPath oldPath = KeyToPath(oldKey);
    XPath newPath = KeyToPath(newKey);
    if (oldPath.Exists())
        oldPath.Rename(newPath);

    SaveIndex();
}

bool DataStore::Contains(const std::string& key) const
{
    std::shared_lock lock(m_Mutex);
    return m_Entries.find(key) != m_Entries.end();
}

std::vector<std::string> DataStore::ListKeys() const
{
    std::shared_lock lock(m_Mutex);

    std::vector<std::string> keys;
    keys.reserve(m_Entries.size());
    for (const auto& pair : m_Entries)
        keys.push_back(pair.first);
    return keys;
}

// ═════════════════════════════════════════════════════════════════════════════
//  持久化
// ═════════════════════════════════════════════════════════════════════════════

bool DataStore::Save(const std::string& key)
{
    std::shared_lock lock(m_Mutex);

    auto it = m_Entries.find(key);
    if (it == m_Entries.end() || !it->second)
        return false;

    EnsureDataDir();

    // 确保父目录存在

    XPath path = KeyToPath(key);
    XPath parent = path.GetParent();
    if (!parent.Exists())
        parent.CreateDirectory();

    return FilesSystem::WriteFileBinary(path, it->second);
}

void DataStore::ClearAll()
{
    std::lock_guard<std::shared_mutex> lock(m_Mutex);
    m_Entries.clear();
}

bool DataStore::SaveAll()
{
    std::shared_lock lock(m_Mutex);

    if (m_Entries.empty())
        return true;

    EnsureDataDir();

    bool allOk = true;
    for (const auto& pair : m_Entries)
    {
        if (!pair.second)
            continue;

        XPath path = KeyToPath(pair.first);
        XPath parent = path.GetParent();
        if (!parent.Exists())
            parent.CreateDirectory();

        if (!FilesSystem::WriteFileBinary(path, pair.second))
            allOk = false;
    }

    SaveIndex();
    return allOk;
}

void DataStore::Flush(const std::string& key)
{
    std::shared_lock lock(m_Mutex);

    auto it = m_Entries.find(key);
    if (it == m_Entries.end() || !it->second)
        return;

    EnsureDataDir();

    XPath path = KeyToPath(key);
    XPath parent = path.GetParent();
    if (!parent.Exists())
        parent.CreateDirectory();

    FilesSystem::AppendFileBinary(path, it->second);
}

void DataStore::FlushAll()
{
    std::shared_lock lock(m_Mutex);

    if (m_Entries.empty())
        return;

    EnsureDataDir();

    for (const auto& pair : m_Entries)
    {
        if (!pair.second)
            continue;

        XPath path = KeyToPath(pair.first);
        XPath parent = path.GetParent();
        if (!parent.Exists())
            parent.CreateDirectory();

        FilesSystem::AppendFileBinary(path, pair.second);
    }
}

bool DataStore::LoadFile(const std::string& filepath)
{
    Buffer buf = FilesSystem::ReadFileBinary(filepath);
    if (!buf)
        return false;

    XPath path(filepath);
    std::string key = path.getName();

    std::unique_lock lock(m_Mutex);
    m_Entries[key] = std::move(buf);
    SaveIndex();

    return true;
}

void DataStore::LoadDirectory(const XPath& dir)
{
    if (!dir.Exists() || !dir.IsDirectory())
        return;

    // 先重建索引
    LoadIndex();

    std::vector<XPath> files = dir.ListDirectory();
    for (const auto& file : files)
    {
        if (file.IsFile() && file.getName() != ".dsidx")
        {
            std::string key = file.getRelativePath(m_DataDir);
            if (m_Entries.find(key) == m_Entries.end())
                m_Entries[key] = Buffer();  // 空占位
        }
    }

    SaveIndex();
}

// ═════════════════════════════════════════════════════════════════════════════
//  统计
// ═════════════════════════════════════════════════════════════════════════════

DataStoreStats DataStore::GetStats() const
{
    std::shared_lock lock(m_Mutex);

    DataStoreStats stats;
    stats.EntryCount = m_Entries.size();
    stats.Entries.reserve(m_Entries.size());

    for (const auto& pair : m_Entries)
    {
        stats.TotalBytes += pair.second.Size;
        stats.TotalCapacity += pair.second.Capacity;

        DataStoreStats::EntryInfo info;
        info.Key = pair.first;
        info.Size = pair.second.Size;
        info.Capacity = pair.second.Capacity;
        stats.Entries.push_back(std::move(info));
    }

    return stats;
}

std::string DataStore::ToString() const
{
    DataStoreStats s = GetStats();

    std::ostringstream oss;
    oss << "=== DataStore Stats ===\n";
    oss << "Entries: " << s.EntryCount << "\n";
    oss << "Total: " << s.TotalBytes << " bytes (data) / "
        << s.TotalCapacity << " bytes (capacity)\n\n";

    for (const auto& info : s.Entries)
    {
        oss << "  [" << info.Key << "] "
            << info.Size << "/" << info.Capacity << " bytes\n";
    }

    oss << "========================\n";
    return oss.str();
}

} // namespace X_Y
