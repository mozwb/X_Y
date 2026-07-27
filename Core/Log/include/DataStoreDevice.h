#pragma once
#include "LogConfigure.h"
#include "DataStore/include/DataStore.h"

namespace X_Y {

// ── DataStoreDevice ──
// Log 设备：每次 Log 调用时推入 DataStore 管理的 RingBuffer
// 内部维护写入计数，满一定量时自动将 RingBuffer 持久化到 DataStore
//
// 前后端分离：
//   - Log 只管写入（Push 到 RingBuffer）
//   - DataStoreDevice 在必要时自动调用 DataStore::FlushRingBuffer(m_Key)
//   - LogViewer 直接从同一 RingBuffer 读取（零拷贝）

class DataStoreDevice : public LogConfigure::DEVICE {
public:
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

    void Log(const std::string& message) const override {
        RingBuffer* ring = DataStore::Instance().GetOrCreateRingBuffer(m_Key, m_Capacity);
        if (!ring)
            return;

        ring->Push(message.data(), message.size());
        ring->Push("\n", 1);

        m_LogCount++;
        if (m_LogCount >= m_FlushCount)
            DataStore::Instance().FlushRingBuffer(m_Key);
    }

    void Flush() const {
        DataStore::Instance().FlushRingBuffer(m_Key);
    }

    void SetKey(const std::string& key) { m_Key = key; }
    const std::string& GetKey() const { return m_Key; }
    void SetCapacity(uint64_t capacity) { m_Capacity = capacity; }
    uint64_t GetCapacity() const { return m_Capacity; }
    void SetFlushCount(uint64_t c) { m_FlushCount = c; }
    uint64_t GetFlushCount() const { return m_FlushCount; }

private:
    static std::string DefaultKey() {
        return SysClock::NowFormat("YY-MM-DD") + "_log";
    }

    mutable std::string m_Key;
    uint64_t m_Capacity;
    uint64_t m_FlushCount = 512;
    mutable uint64_t m_LogCount = 0;
};

} // namespace X_Y
