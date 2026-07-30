#pragma once
#include "LogConfigure.h"
#include "DataStore/include/DataStore.h"
#include "FilesSystem/include/FilesSystem.h"

namespace X_Y {

// ── DataStoreDevice ──
// Log 设备：每次 Log 调用时追加到 DataStore 管理的固定容量 Buffer
// 写满时自动 flush 到文件后重置，从头继续写
// 本质就是个 append-only 定长队列
//
// 前后端分离：
//   - Log 只管写入（Append 到 Buffer）
//   - DataStoreDevice 满了自动 Flush 到文件后重置
//   - LogViewer 直接从同一 Buffer 读取（零拷贝）


class DataStoreDevice : public LogConfigure::DEVICE {
public:
    explicit DataStoreDevice(const std::string& key = "",
                              uint64_t capacity = 65536)
        : m_Key(key.empty() ? DefaultKey() : key)
        , m_Capacity(capacity)
    {
    }

    std::string toString() const override {
        return "DataStoreBuf[" + m_Key + "]";
    }

    void Log(const std::string& message) const override {
        Buffer* buf = DataStore::Instance().GetOrCreate(m_Key, m_Capacity);
        if (!buf)
            return;

        uint64_t needed = message.size() + 1;
        if (buf->Size + needed > buf->Capacity) {
            DataStore::Instance().Flush(m_Key);
            buf->Allocate(0);  // 重置为空 Buffer，下次 Log 写入从头
        }

        buf->Append(message.data(), message.size());
        buf->Append("\n", 1);
    }

    ~DataStoreDevice() {
        // 程序退出时不 Flush，此时 DataStore 可能已析构
		DataStore::Instance().Flush(m_Key);
    }

    void SetKey(const std::string& key) { m_Key = key; }
    const std::string& GetKey() const { return m_Key; }
    void SetCapacity(uint64_t capacity) { m_Capacity = capacity; }
    uint64_t GetCapacity() const { return m_Capacity; }

private:
    static std::string DefaultKey() {
        return SysClock::NowFormat("YY-MM-DD") + ".log";
    }

    mutable std::string m_Key;
    uint64_t m_Capacity;
};

} // namespace X_Y
