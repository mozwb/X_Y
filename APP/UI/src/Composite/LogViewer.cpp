#include "Composite/LogViewer.h"
#include "DataStore/include/DataStore.h"
#include "Timer/include/Timer.h"
#include "Widget/include/BaseWin.h"
#include <algorithm>
#include <cctype>

namespace X_Y {

void LogStripe::SetEntries(const std::vector<LogEntry>& entries) {
    Clear();
    for (const auto& entry : entries) {
        AddItem(entry.text.c_str(), entry.color);
    }
}

// ════════════════════════════════════════════════════════════
// 工具：解析单行日志
// ════════════════════════════════════════════════════════════

static LogEntry ParseLine(const std::string& raw) {
    LogEntry entry;
    entry.color = 0xFF000000;

    size_t p1 = raw.find('%');
    if (p1 == 0) {
        size_t p2 = raw.find('%', 1);
        if (p2 != std::string::npos) {
            std::string colorStr = raw.substr(p1 + 1, p2 - p1 - 1);
            int r = 0, g = 0, b = 0;
            if (sscanf_s(colorStr.c_str(), "%d:%d:%d", &r, &g, &b) == 3)
                entry.color = 0xFF000000 | (r << 16) | (g << 8) | b;

            entry.text = raw.substr(p2 + 1);
            auto pos = entry.text.rfind("%#");
            if (pos != std::string::npos)
                entry.text = entry.text.substr(0, pos);
            return entry;
        }
    }

    entry.text = raw;
    return entry;
}

// ════════════════════════════════════════════════════════════
// 构造 / 析构
// ════════════════════════════════════════════════════════════

LogViewer::LogViewer() {
    m_Key = SysClock::NowFormat("YY-MM-DD") + ".log";

    m_KeywordInput = std::make_unique<TextInput>();
    m_KeywordInput->OnTextChange = [this](const std::string& text) {
        OnKeywordChanged(text);
    };

    m_LogStripe = std::make_unique<LogStripe>();

    m_ScrollArea = std::make_unique<ScrollArea>();
    m_ScrollArea->SetContent(m_LogStripe.get());

    AddComponent(m_KeywordInput.get());
    AddComponent(m_ScrollArea.get());
}

LogViewer::~LogViewer() {
    Stop();
}

// ════════════════════════════════════════════════════════════
// 配置
// ════════════════════════════════════════════════════════════

void LogViewer::SetDataStoreKey(const std::string& key) {
    {
        std::lock_guard<std::shared_mutex> lock(m_EntriesMutex);
        m_Key = key;
        m_LastSize = 0;
        m_AllEntries.clear();
    }
    RequestRepaint();
}

// ════════════════════════════════════════════════════════════
// 生命周期
// ════════════════════════════════════════════════════════════

void LogViewer::Start() {
    if (m_Timer) return;

    m_Timer = std::make_unique<Ticker>();
    m_Timer->Start(500, [this]() {
        Buffer* buf = DataStore::Instance().Get(m_Key);
        if (!buf || !buf->Data || buf->Size == 0) return;

        uint64_t currentSize = buf->Size;

        std::lock_guard<std::shared_mutex> lock(m_EntriesMutex);

        if (currentSize < m_LastSize) {
            m_LastSize = 0;
            m_AllEntries.clear();
        }

        if (currentSize == m_LastSize) return;

        auto tailLen = currentSize - m_LastSize;
        auto* bytes = reinterpret_cast<const char*>(buf->Data) + m_LastSize;
        m_LastSize = currentSize;

        std::string current;
        for (uint64_t i = 0; i < tailLen; i++) {
            char c = bytes[i];
            if (c == '\n') {
                if (!current.empty()) {
                    while (m_AllEntries.size() >= MAX_ENTRIES)
                        m_AllEntries.pop_front();
                    m_AllEntries.push_back(ParseLine(current));
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            while (m_AllEntries.size() >= MAX_ENTRIES)
                m_AllEntries.pop_front();
            m_AllEntries.push_back(ParseLine(current));
        }

        RequestRepaint();
    });
}

void LogViewer::Stop() {
    if (!m_Timer) return;
    m_Timer->Stop();
    m_Timer->Join();
    m_Timer.reset();
}

// ════════════════════════════════════════════════════════════
// 筛选（持有共享锁时调用）
// ════════════════════════════════════════════════════════════

void LogViewer::ApplyFilter() {
    m_LogStripe->Clear();

    for (const auto& entry : m_AllEntries) {
        if (!m_Keyword.empty()) {
            std::string lowerText = entry.text;
            std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lowerText.find(m_Keyword) == std::string::npos)
                continue;
        }

        m_LogStripe->AddItem(entry.text.c_str(), entry.color);
    }
}

// ════════════════════════════════════════════════════════════
// 交互回调
// ════════════════════════════════════════════════════════════

void LogViewer::OnKeywordChanged(const std::string& text) {
    m_Keyword = text;
    std::transform(m_Keyword.begin(), m_Keyword.end(),
                   m_Keyword.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    RequestRepaint();
}

// ════════════════════════════════════════════════════════════
// 绘制（主线程）
// ════════════════════════════════════════════════════════════

void LogViewer::OnPaint(Canvas& canvas) {
    LayoutChildren();

    canvas.FillRect(0, 0, get_width(), get_height(), 0xFF1E1E1E);

    // 共享锁保护 m_AllEntries 的读取 + 筛选
    {
        std::shared_lock<std::shared_mutex> lock(m_EntriesMutex);
        ApplyFilter();
    }

    Container::OnPaint(canvas);
}

// ════════════════════════════════════════════════════════════
// 布局
// ════════════════════════════════════════════════════════════

void LogViewer::LayoutChildren() {
    int w = get_width();
    int h = get_height();
    if (w <= 0 || h <= 0) return;

    const int inputH = 22;
    m_KeywordInput->SetRect(0, 0, w, inputH);

    const int panelY = inputH + 1;
    const int panelH = h - panelY;
    if (panelH > 0)
        m_ScrollArea->SetRect(0, panelY, w, panelH);
}

}
// namespace X_Y
