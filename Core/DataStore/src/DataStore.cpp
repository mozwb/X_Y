#include "DataStore/include/DataStore.h"
#include <cassert>
#include <iostream>

namespace X_Y {

// ── 单例 ──

DataStore& DataStore::Instance()
{
    static DataStore inst;
    return inst;
}

DataStore::DataStore()
{
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

// ── 前后端分离：统一内存管理 ──

Buffer* DataStore::GetOrCreate(const std::string& key, uint64_t reserveSize)
{
    auto it = m_Entries.find(key);
    if (it != m_Entries.end())
        return &it->second;

    Buffer buf = m_Pool.Allocate(reserveSize);
    auto result = m_Entries.emplace(key, std::move(buf));
    return &result.first->second;
}

RingBuffer* DataStore::GetOrCreateRingBuffer(const std::string& key, uint64_t capacity)
{
    auto it = m_RingBuffers.find(key);
    if (it != m_RingBuffers.end())
        return &it->second;

    auto result = m_RingBuffers.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(key),
        std::forward_as_tuple(capacity)
    );
    return &result.first->second;
}

// ── 持久化 ──

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

// ── 统计 ──

DataStore::Stats DataStore::GetStats() const
{
    Stats stats;

    stats.PoolBlockSize = m_Pool.BlockSize();
    stats.PoolTotalBlocks = m_Pool.TotalBlocks();
    stats.PoolFreeBlocks = m_Pool.FreeBlocks();
    stats.PoolUsedBytes = m_Pool.UsedBytes();

    stats.TotalEntries = m_Entries.size();
    stats.RingBufferCount = m_RingBuffers.size();

    for (const auto& pair : m_Entries)
    {
        const Buffer& buf = pair.second;
        stats.TotalBytes += buf.Size;
        stats.TotalCapacity += buf.Capacity;

        if (IsPoolBuffer(buf))
            stats.PoolEntries++;
        else
            stats.SelfManagedEntries++;
    }

    return stats;
}

void DataStore::DumpStats()
{
    Stats s = GetStats();

    std::cout << "=== DataStore Stats ===" << std::endl;
    std::cout << "Buffer entries: " << s.TotalEntries
              << " (pool: " << s.PoolEntries
              << ", self: " << s.SelfManagedEntries << ")" << std::endl;
    std::cout << "  Total data: " << s.TotalBytes << " bytes" << std::endl;
    std::cout << "  Total capacity: " << s.TotalCapacity << " bytes" << std::endl;
    std::cout << "RingBuffers: " << s.RingBufferCount << std::endl;
    std::cout << "Pool: " << s.PoolBlockSize << " bytes/block, "
              << "total " << s.PoolTotalBlocks << " blocks, "
              << "free " << s.PoolFreeBlocks << ", "
              << "used " << s.PoolUsedBytes << " bytes" << std::endl;

    // 详细展开
    for (const auto& pair : m_Entries) {
        const Buffer& buf = pair.second;
        const char* src = IsPoolBuffer(buf) ? "pool" : "self";
        std::cout << "  [" << pair.first << "] "
                  << buf.Size << "/" << buf.Capacity << " bytes (" << src << ")"
                  << std::endl;
    }
    for (const auto& pair : m_RingBuffers) {
        std::cout << "  Ring[" << pair.first << "] "
                  << pair.second.Capacity() << " bytes, "
                  << pair.second.TotalBlocks() << " blocks"
                  << std::endl;
    }
    std::cout << "========================" << std::endl;
}

} // namespace X_Y
