#pragma once
#include "LogConfigure.h"
#include "DataStore/include/DataStore.h"

namespace X_Y {

// ── DataStoreDevice ──
// Log 设备：每次 Log 调用时推入 DataStore 管理的 RingBuffer
// 内部维护写入计数，满一定量时自动 Flush 到文件
//
// 前后端分离：
//   - Log 只管写入（Push 到 RingBuffer）
//   - LogViewer 直接从同一 RingBuffer 读取（零拷贝）
//   - Flush 策略由 DataStoreDevice 自己判定
//
// 使用方式：
//   logger.add(new DataStoreDevice());                      // 默认 key，满 512 条自动 Flush
//   logger.add(new DataStoreDevice("my_log", 128 * 1024));  // 自定义容量

class DataStoreDevice : public LogConfigure::DEVICE {
public:
    // ── 构造 ──
    // key:        RingBuffer 在 DataStore 中的名字
    // capacity:   RingBuffer 容量（字节），默认 64KB
    // flushCount: 每写 flushCount 条日志自动 Flush 一次（默认 512）
    DataStoreDevice(const std::string& key = "",
                    uint64_t capacity = 65536,
                    uint64_t flushCount = 512)
        : m_Key(key.empty() ? DefaultKey() : key)
        , m_Capacity(capacity)
        , m_FlushCount(flushCount)
    {
    }

    std::string toString() const override {
        return "DataStoreRing[" + m_Key + "]";
    }

    // ── Log 调用 ──
    void Log(const std::string& message) const override {
        RingBuffer* ring = DataStore::Instance().GetOrCreateRingBuffer(m_Key, m_Capacity);
        if (!ring)
            return;

        ring->Push(message.data(), message.size());
        ring->Push("\n", 1);

        m_LogCount++;

        // 满 flushCount 条自动 Flush
        if (m_LogCount >= m_FlushCount)
            FlushToFile(ring);
    }

    // ── 手动 Flush ──
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
    void SetFlushCount(uint64_t c) { m_FlushCount = c; }
    uint64_t GetFlushCount() const { return m_FlushCount; }

private:
    void FlushToFile(RingBuffer* ring) const {
        // 读 RingBuffer 全部数据
        Buffer collected;
        ring->Read([&](const uint8_t* data, uint64_t size) {
            collected.Append(data, size);
        });

        if (collected.Size > 0) {
            // 追加写入文件
            DataStore& ds = DataStore::Instance();
            std::string filename = m_Key + ".log";
            XPath path = ds.GetDataDir() / filename;

            if (!ds.GetDataDir().Exists())
                ds.GetDataDir().CreateDirectory();

            FilesSystem::AppendFileBinary(path, collected);
        }

        // 清空 RingBuffer，重置计数
        ring->Clear();
        m_LogCount = 0;
    }

    static std::string DefaultKey() {
        return SysClock::NowFormat("YY-MM-DD") + "_log";
    }

    mutable std::string m_Key;
    uint64_t m_Capacity;
    uint64_t m_FlushCount = 512;
    mutable uint64_t m_LogCount = 0;  // 写入计数器
};

} // namespace X_Y
