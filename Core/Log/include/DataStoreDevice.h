#pragma once
#include "LogConfigure.h"
#include "DataStore/include/DataStore.h"

namespace X_Y {

// ── DataStoreDevice ──
// Log 设备：每次 Log 调用时推入 DataStore 管理的 RingBuffer
// RingBuffer 固定容量，满了自动覆盖旧日志
//
// 前后端分离：
//   - Log 只管写入（Push 到 RingBuffer）
//   - LogViewer 直接从同一 RingBuffer 读取（零拷贝）
//
// 使用方式：
//   logger.add(new DataStoreDevice());             // 默认 key = "today_log"
//   logger.add(new DataStoreDevice("my_custom_log"));

class DataStoreDevice : public LogConfigure::DEVICE {
public:
    // ── 构造 ──
    // key: RingBuffer 在 DataStore 中的名字
    // capacity: RingBuffer 容量（字节），默认 64KB
    DataStoreDevice(const std::string& key = "", uint64_t capacity = 65536)
        : m_Key(key.empty() ? DefaultKey() : key)
        , m_Capacity(capacity)
    {
    }

    // ── toString（给 Log 设备列表显示用） ──
    std::string toString() const override {
        return "DataStoreRing[" + m_Key + "]";
    }

    // ── Log 调用 ──
    // 每次写入一条日志，推入 RingBuffer
    void Log(const std::string& message) const override {
        RingBuffer* ring = DataStore::Instance().GetOrCreateRingBuffer(m_Key, m_Capacity);
        if (ring) {
            ring->Push(message.data(), message.size());
            ring->Push("\n", 1);  // 换行
        }
    }

    // ── 配置 ──
    void SetKey(const std::string& key) { m_Key = key; }
    const std::string& GetKey() const { return m_Key; }
    void SetCapacity(uint64_t capacity) { m_Capacity = capacity; }
    uint64_t GetCapacity() const { return m_Capacity; }

private:
    // 生成默认 key
    static std::string DefaultKey() {
        return SysClock::NowFormat("YY-MM-DD") + "_log";
    }

    mutable std::string m_Key;
    uint64_t m_Capacity;
};

} // namespace X_Y
