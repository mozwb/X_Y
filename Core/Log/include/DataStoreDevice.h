#pragma once
#include "LogConfigure.h"
#include "DataStore/include/DataStore.h"

namespace X_Y {

// ── DataStoreDevice ──
// Log 设备：每次 Log 调用时追加到 DataStore
// 文件名格式：YYYY-MM-DD.log（按日期自动切换）
//
// 使用方式：
//   logger.add(new DataStoreDevice());  // 默认 "today.log"
//   logger.add(new DataStoreDevice("my_custom_key"));

class DataStoreDevice : public LogConfigure::DEVICE {
public:
   
    // ── 构造 ──
    // 不传 key 时自动用当天日期（格式：YYYY-MM-DD.log）

    DataStoreDevice(const std::string& key = "")
        : m_Key(key.empty() ? DefaultKey() : key)
    {
    }

    // ── toString（给 Log 设备列表显示用） ──
    
    std::string toString() const override {
        return "DataStore[" + m_Key + "]";
    }

   
    // ── Log 调用（每次写一条日志就 Append） ──
    
    void Log(const std::string& message) const override {
        Buffer line(message.size());
        line.Append(message.data(), message.size());
		line.Append("\n", 1); // 换行
        DataStore::Instance().Insert(m_Key, line);
        DataStore::Instance().Flush(m_Key);
    }

    // ── 设置/获取 key ──
    
    void SetKey(const std::string& key) { m_Key = key; }
    const std::string& GetKey() const { return m_Key; }

private:
    // 生成默认 key：YYYY-MM-DD.log
    
    static std::string DefaultKey() {
        return SysClock::NowFormat("YY-MM-DD") + ".log";
    }

    mutable std::string m_Key;  // mutable 因为 Log() 是 const
};

} 
// namespace X_Y
