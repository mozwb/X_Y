#pragma once
#include "UI/include/Container/Container.h"
#include "UI/include/Component/ListBox.h"
#include "UI/include/Component/ScrollArea.h"
#include "UI/include/Component/TextInput.h"
#include "Memory/include/LoopQueue.h"
#include <string>
#include <memory>
#include <cstdint>
#include <shared_mutex>

namespace X_Y {

class Ticker;


// ── LogEntry ──
struct LogEntry {
    std::string text;
    uint32_t    color = 0xFF000000;   // 默认黑色 ARGB
};

// ── LogStripe ──
class LogStripe : public ListBox {
public:
    LogStripe() = default;
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
    void OnKeywordChanged(const std::string& text);
    bool MatchesKeyword(const LogEntry& e) const;   // 单条是否命中关键词
    void RebuildAll();     // 全量重建（关键词变 / 数据重置时）
    void IncrementalAppend(uint64_t fromSeq, uint64_t toSeq); // 增量喂新条目
    void LayoutChildren();

    std::unique_ptr<TextInput>  m_KeywordInput;
    std::unique_ptr<ScrollArea> m_ScrollArea;
    std::unique_ptr<LogStripe>  m_LogStripe;

    static constexpr uint64_t MAX_ENTRIES = 5000;

    LoopQueue<LogEntry, MAX_ENTRIES> m_AllEntries;
    mutable std::shared_mutex m_EntriesMutex;

    std::string m_Key;
    std::string m_Keyword;
    uint64_t m_LastSize = 0;
    uint64_t m_RenderedSeq = 0;  // 已喂入 stripe 的全局序号


    std::unique_ptr<Ticker> m_Timer;
};

} 
// namespace X_Y
