#pragma once
#include "Buffer/include/Buffer.h"
#include "Buffer/include/BufferPool.h"
#include "Buffer/include/RingBuffer.h"
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

    // ── 前后端分离：统一内存管理 ──
    // 获取或创建一块 Buffer（从 BufferPool 分配）
    // 典型用途：Log 组件向 DataStore 要一块 buffer 用于写入
    Buffer* GetOrCreate(const std::string& key, uint64_t reserveSize = 4096);

    // 获取或创建一个 RingBuffer
    // 典型用途：日志环形缓冲区，Log 写入，LogViewer 读取
    RingBuffer* GetOrCreateRingBuffer(const std::string& key, uint64_t capacity = 65536);

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
    void DumpStats();

private:
    DataStore();

    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    XPath KeyToPath(const std::string& key) const;
    bool LoadFileInto(const std::string& key, const XPath& path);

    std::unordered_map<std::string, Buffer>     m_Entries;
    std::unordered_map<std::string, RingBuffer> m_RingBuffers;
    BufferPool m_Pool{65536};  // 通用内存池，每块 64KB

    XPath m_DataDir{"DataStore/"};
};

} // namespace X_Y
