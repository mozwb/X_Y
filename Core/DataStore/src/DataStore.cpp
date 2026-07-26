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

void DataStore::Insert(const std::string& key, Buffer data)
{
    m_Entries[key] = std::move(data);
    Flush(key);  // Insert 自动写盘
}

void DataStore::Append(const std::string& key, const Buffer& data)
{
    auto it = m_Entries.find(key);
    if (it != m_Entries.end()) {
        // 已有数据，追加
        it->second.Append(data.Data, data.Size);
    } else {
        // 新 key，直接复制
        m_Entries[key] = data.Copy();
    }
    Flush(key);  // Append 自动写盘
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
    // 删文件
    XPath path = KeyToPath(key);
    if (path.Exists())
        path.Remove();

    // 删内存
    return m_Entries.erase(key) > 0;
}

void DataStore::Rename(const std::string& oldKey, const std::string& newKey)
{
    auto it = m_Entries.find(oldKey);
    if (it == m_Entries.end())
        return;

    // 搬内存
    m_Entries[newKey] = std::move(it->second);
    m_Entries.erase(it);

    // 搬文件
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

void DataStore::Flush(const std::string& key)
{
    auto it = m_Entries.find(key);
    if (it == m_Entries.end())
        return;

    // 确保目录存在
    if (!m_DataDir.Exists())
        m_DataDir.CreateDirectory();

    XPath path = KeyToPath(key);
    FilesSystem::WriteFileBinary(path, it->second);
}

void DataStore::FlushAll()
{
    if (m_Entries.empty())
        return;

    // 确保目录存在
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

    // 提取文件名作为 key（不含路径和后缀）
    XPath path(filepath);
    std::string name = path.getName();  // "teapot.bin"

    // 去掉扩展名
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
