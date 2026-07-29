#pragma once
#include "Container/Container.h"
#include "Component/ListBox.h"
#include "Component/ScrollArea.h"
#include "Component/TextInput.h"
#include <deque>
#include <string>
#include <memory>
#include <cstdint>

namespace X_Y {

class Ticker;


// ── LogStripe ──
// 纯展示层：接收解析好的 (text, color) 条目列表并显示
// 不做数据采集、不做解析、不做筛选，只负责渲染

struct LogEntry {
    std::string text;
    uint32_t    color = 0xFF000000;   // 默认黑色 ARGB
};

class LogStripe : public ListBox {
public:
    LogStripe() = default;

    // 清空旧内容 + 填入新条目
    void SetEntries(const std::vector<LogEntry>& entries);
};

class LogViewer : public Container {
public:
    LogViewer();
    ~LogViewer() override;

    void SetDataStoreKey(const std::string& key);
    const std::string& GetDataStoreKey() const { return m_Key; }

    void Start();
    void Stop();

protected:
    void OnPaint(Canvas& canvas) override;
    void OnPaint(Canvas* canvas) override { if (canvas) OnPaint(*canvas); }

private:
    struct CachedEntry {
        std::string text;
        uint32_t    color;
    };

    void OnIncrementalData(const std::string& rawTail);
    void ApplyFilter();
    void OnKeywordChanged(const std::string& text);
    void LayoutChildren();

    std::unique_ptr<TextInput>  m_KeywordInput;
    std::unique_ptr<ScrollArea> m_ScrollArea;
    std::unique_ptr<LogStripe>  m_LogStripe;

    std::string m_Key;
    std::deque<CachedEntry> m_AllEntries;
    std::string m_Keyword;
    uint64_t m_LastSize = 0;

    static constexpr uint64_t MAX_ENTRIES = 5000;

    std::unique_ptr<Ticker> m_Timer;
};

} // namespace X_Y
