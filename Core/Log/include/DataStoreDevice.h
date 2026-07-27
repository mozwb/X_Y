#pragma once
#include "LogConfigure.h"
#include "DataStore/include/DataStore.h"

namespace X_Y {

// ── DataStoreDevice ──
// Log 设备：每次 Log 调用时推入 DataStore 管理的 RingBuffer
// RingBuffer 固定容量，超阈值时自动 Flush 到文件以避免数据丢失
//
// 前后端分离：
//   - Log 只管写入（Push 到 RingBuffer）
//   - DataStoreDevice 在必要时自动 Flush 到文件
//   - LogViewer 直接从同一 RingBuffer 读取（零拷贝）
//
// 使用方式：
//   logger.add(new DataStoreDevice());                       // 默认 key + 64KB
//   logger.add(new DataStoreDevice("my_log", 128 * 1024));   // 自定义

class DataStoreDevice : public LogConfigure::DEVICE {
public:
    // ── 构造 ──
    // key: RingBuffer 在 DataStore 中的名字
    // capacity: RingBuffer 容量（字节），默认 64KB
    // flushThreshold: 填充率超过此值则自动 Flush（默认 0.8 = 80%）
    DataStoreDevice(const std::string& key = "",
                    uint64_t capacity = 65536,
                    double flushThreshold = 0.8)
        : m_Key(key.empty() ? DefaultKey() : key)
        , m_Capacity(capacity)
        , m_FlushThreshold(flushThreshold)
    {
    }

    // ── toString（给 Log 设备列表显示用） ──
    std::string toString() const override {
        return "DataStoreRing[" + m_Key + "]";
    }

    // ── Log 调用 ──
    // 推入 RingBuffer，填充率超过阈值则 Flush 到文件 + Clear
    void Log(const std::string& message) const override {
        RingBuffer* ring = DataStore::Instance().GetOrCreateRingBuffer(m_Key, m_Capacity);
        if (!ring)
            return;

        ring->Push(message.data(), message.size());
        ring->Push("\n", 1);

        // 检查填充率，超阈值则 Flush
        if (ring->FillRatio() >= m_FlushThreshold)
            FlushToFile(ring);
    }

    // ── 手动 Flush ──
    // 显式将当前 RingBuffer 内容写入文件
    void Flush() const {
        RingBuffer* ring = DataStore::Instance().GetOrCreateRingBuffer(m_Key, m_Capacity);
        if (ring)
            FlushToFile(ring);
    }

    // ── 配置 ──
    void SetKey(const std::string& key) { m_Key = key; }
    const std::string& GetKey() const { return m_Key; }
    void SetCapacity(uint64_t capacity) { m_Capacity = capacity; }
    uint64_t GetCapacity() const { return m_Capacity; }
    void SetFlushThreshold(double t) { m_FlushThreshold = t; }
    double GetFlushThreshold() const { return m_FlushThreshold; }

private:
    // 将 RingBuffer 内容追加写入文件
    void FlushToFile(RingBuffer* ring) const {
        // 收集所有数据到一个 Buffer
        Buffer collected;
        ring->Read([&](const uint8_t* data, uint64_t size) {
            collected.Append(data, size);
        });

        if (collected.Size == 0)
            return;

        // 以 date_key.log 为文件名写入 DataStore 目录
        DataStore& ds = DataStore::Instance();
        std::string filename = m_Key + ".log";
        XPath path = ds.GetDataDir() / filename;

        if (!ds.GetDataDir().Exists())
            ds.GetDataDir().CreateDirectory();

        FilesSystem::AppendFileBinary(path, collected);

        // 清空 RingBuffer，继续接收新日志
        ring->Clear();
    }

    static std::string DefaultKey() {
        return SysClock::NowFormat("YY-MM-DD") + "_log";
    }

    mutable std::string m_Key;
    uint64_t m_Capacity;
    double   m_FlushThreshold = 0.8;
};

} // namespace X_Y
