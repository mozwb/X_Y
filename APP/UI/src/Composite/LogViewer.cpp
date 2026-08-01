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

// 解析 Log 写入的 ANSI 颜色序列，剥离控制字符，提取前景色
// 支持的序列：\x1B[38;2;R;G;Bm(前景RGB) / \x1B[...m(其他，仅剥离)
// 注意：\x1B[0m(重置) 不覆盖行色——Log 每行结构为 <颜色>正文<重置>，
//       行色按该行设置的第一个前景色为准，结尾重置只是收尾，不把整行抹黑。
static LogEntry ParseLine(const std::string& raw) {
    LogEntry entry;
    entry.color = 0xFF000000;
    bool hasColor = false;

    std::string text;
    text.reserve(raw.size());

    size_t i = 0;
    const size_t n = raw.size();
    while (i < n) {
        if (raw[i] == '\x1B' && i + 1 < n && raw[i + 1] == '[') {
            size_t j = i + 2;
            while (j < n && raw[j] != 'm') j++;   // 找到序列结束符 'm'
            if (j >= n) break;                     // 未闭合，丢弃剩余

            std::string esc = raw.substr(i + 2, j - i - 2); // 不含 ESC[ 和 m 的参数字符串
            i = j + 1;

            // \x1B[0m 重置：只作为收尾标记，不覆盖已定的行色
            if (esc == "0" || esc == "0;0")
                continue;

            // 前景色 RGB：38;2;R;G;B
            if (esc.rfind("38;2;", 0) == 0) {
                int r = 0, g = 0, b = 0;
                if (sscanf_s(esc.c_str() + 5, "%d;%d;%d", &r, &g, &b) == 3) {
                    entry.color = 0xFF000000 | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
                    hasColor = true;
                }
            }
            // 其他序列(背景/粗体等)仅剥离，不改变颜色
            continue;
        }

        text += raw[i];
        i++;
    }

    entry.text = std::move(text);
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
