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

    // ── 统计信息 ──
    struct Stats {
        uint64_t TotalEntries = 0;       // m_Entries 总数
        uint64_t PoolEntries = 0;        // 来自 Pool 的条目数
        uint64_t SelfManagedEntries = 0; // 自管/外来挂载的条目数
        uint64_t TotalBytes = 0;         // 所有 Buffer 的 Size 之和
        uint64_t TotalCapacity = 0;      // 所有 Buffer 的 Capacity 之和

        uint64_t RingBufferCount = 0;
        uint64_t PoolBlockSize = 0;
        uint32_t PoolTotalBlocks = 0;
        uint32_t PoolFreeBlocks = 0;
        uint64_t PoolUsedBytes = 0;
    };

    Stats GetStats() const;

    // ── 数据库操作 ──
    // Insert：外部挂载一个 Buffer 到 DataStore
    // 注意：不接受 Pool Buffer（带 Deleter 的 Buffer）
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
    // 调用方直接追加写入即可
    Buffer* GetOrCreate(const std::string& key, uint64_t reserveSize = 4096);

    // 获取或创建一个 RingBuffer
    RingBuffer* GetOrCreateRingBuffer(const std::string& key, uint64_t capacity = 65536);

    // ── 自管 Buffer 分配（不走 Pool）──
    // 适合 Image、Model 等一次性大数据
    // 已有则重新分配大小，没有则创建
    Buffer* CreateBuffer(const std::string& key, uint64_t size);

    // ── 持久化 ──
    bool Save(const std::string& key);
    bool SaveAll();
    void Flush(const std::string& key);
    void FlushAll();

    // ── RingBuffer 持久化 ──
    // 将指定 key 的 RingBuffer 内容追加写入文件
    // 文件名为 {key}.log，位于 DataStore 目录下
    bool FlushRingBuffer(const std::string& key);
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

    // 检查一个 Buffer 是否由 Pool 管理
    static bool IsPoolBuffer(const Buffer& buf) { return (bool)buf.Deleter; }

    XPath KeyToPath(const std::string& key) const;
    bool LoadFileInto(const std::string& key, const XPath& path);

    std::unordered_map<std::string, Buffer>     m_Entries;
    std::unordered_map<std::string, RingBuffer> m_RingBuffers;
    BufferPool m_Pool{65536};  // 通用内存池，每块 64KB

    XPath m_DataDir{"DataStore/"};
};

} // namespace X_Y
