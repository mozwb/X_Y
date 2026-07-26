#include "DataStore/include/DataStore.h"
#include <cassert>

namespace X_Y {

// ── 单例 ──

DataStore& DataStore::Instance()
{
    static DataStore inst;
    return inst;
}

// ── 内部工具 ──

XPath DataStore::KeyToPath(const std::string& key) const
{
    return m_DataDir / key;
}

bool DataStore::LoadFileInto(const std::string& key, const XPath& path)
{
    Buffer buf = FilesSystem::ReadFileBinary(path);
    if (!buf)
        return false;
    m_Entries[key] = std::move(buf);
    return true;
}

// ── 配置 ──

void DataStore::SetDataDir(const std::string& dir)
{
    m_DataDir = dir;
}

// ── 数据库操作 ──
// Insert/Append 只操作内存，不碰文件
// 需要持久化时手动调 Save/Flush

void DataStore::Insert(const std::string& key, Buffer data)
{
    m_Entries[key] = std::move(data);
}

void DataStore::Append(const std::string& key, const Buffer& data)
{
    auto it = m_Entries.find(key);
    if (it != m_Entries.end()) {
        it->second.Append(data.Data, data.Size);
    } else {
        m_Entries[key] = data.Copy();
    }
}

Buffer* DataStore::Get(const std::string& key)
{
    auto it = m_Entries.find(key);
    if (it != m_Entries.end())
        return &it->second;

    // 内存没有，尝试从文件加载
    XPath path = KeyToPath(key);
    if (path.Exists()) {
        if (LoadFileInto(key, path)) {
            auto it2 = m_Entries.find(key);
            if (it2 != m_Entries.end())
                return &it2->second;
        }
    }

    return nullptr;
}

const Buffer* DataStore::Get(const std::string& key) const
{
    auto it = m_Entries.find(key);
    if (it != m_Entries.end())
        return &it->second;
    return nullptr;
}

bool DataStore::Remove(const std::string& key)
{
    XPath path = KeyToPath(key);
    if (path.Exists())
        path.Remove();
    return m_Entries.erase(key) > 0;
}

void DataStore::Rename(const std::string& oldKey, const std::string& newKey)
{
    auto it = m_Entries.find(oldKey);
    if (it == m_Entries.end())
        return;

    m_Entries[newKey] = std::move(it->second);
    m_Entries.erase(it);

    XPath oldPath = KeyToPath(oldKey);
    XPath newPath = KeyToPath(newKey);
    if (oldPath.Exists())
        oldPath.Rename(newPath);
}

bool DataStore::Contains(const std::string& key) const
{
    return m_Entries.find(key) != m_Entries.end();
}

std::vector<std::string> DataStore::ListKeys() const
{
    std::vector<std::string> keys;
    keys.reserve(m_Entries.size());
    for (const auto& pair : m_Entries)
        keys.push_back(pair.first);
    return keys;
}

// ── 持久化 ──
// Save: 覆盖写（全量快照）
// Flush: 追加写（适合日志累积）

bool DataStore::Save(const std::string& key)
{
    auto it = m_Entries.find(key);
    if (it == m_Entries.end())
        return false;

    if (!m_DataDir.Exists())
        m_DataDir.CreateDirectory();

    XPath path = KeyToPath(key);
    return FilesSystem::WriteFileBinary(path, it->second);
}

bool DataStore::SaveAll()
{
    if (m_Entries.empty())
        return true;

    if (!m_DataDir.Exists())
        m_DataDir.CreateDirectory();

    bool allOk = true;
    for (const auto& pair : m_Entries) {
        if (!Save(pair.first))
            allOk = false;
    }
    return allOk;
}

void DataStore::Flush(const std::string& key)
{
    auto it = m_Entries.find(key);
    if (it == m_Entries.end())
        return;

    if (!m_DataDir.Exists())
        m_DataDir.CreateDirectory();

    XPath path = KeyToPath(key);
    FilesSystem::AppendFileBinary(path, it->second);
}

void DataStore::FlushAll()
{
    if (m_Entries.empty())
        return;

    if (!m_DataDir.Exists())
        m_DataDir.CreateDirectory();

    for (const auto& pair : m_Entries)
        Flush(pair.first);
}

bool DataStore::LoadFile(const std::string& filepath)
{
    Buffer buf = FilesSystem::ReadFileBinary(filepath);
    if (!buf)
        return false;

    XPath path(filepath);
    std::string name = path.getName();

    auto dotPos = name.rfind('.');
    if (dotPos != std::string::npos)
        name = name.substr(0, dotPos);

    m_Entries[name] = std::move(buf);
    return true;
}

void DataStore::LoadDirectory(const XPath& dir)
{
    if (!dir.Exists() || !dir.IsDirectory())
        return;

    std::vector<XPath> files = dir.ListDirectory();
    for (const auto& file : files) {
        if (file.IsFile())
            LoadFile(file.Path().string());
    }
}

} 
// namespace X_Y
