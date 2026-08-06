#include "Composite/LogViewer.h"
#include "DataStore/include/DataStore.h"
#include "Timer/include/Timer.h"
#include "Widget/include/BaseWin.h"

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
        m_AllEntries.Clear();
        m_LogStripe->Clear();       // 换数据源 → stripe 同步清空
        m_RenderedSeq = 0;          // 序号归零
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
            m_AllEntries.Clear();
            // 数据被重置（换了文件/清空），stripe 也全量重建
            RebuildAll();
        }

        if (currentSize == m_LastSize) return;

        auto tailLen = currentSize - m_LastSize;
        auto* bytes = reinterpret_cast<const char*>(buf->Data) + m_LastSize;
        m_LastSize = currentSize;

        // 入队前的全局序号，用于增量喂入范围
        uint64_t before = m_AllEntries.TotalPushed();

        std::string current;
        for (uint64_t i = 0; i < tailLen; i++) {
            char c = bytes[i];
            if (c == '\n') {
                if (!current.empty()) {
                    m_AllEntries.Push(ParseLine(current));
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            m_AllEntries.Push(ParseLine(current));
        }

        uint64_t after = m_AllEntries.TotalPushed();
        IncrementalAppend(before, after);
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

// ════════════════════════════════════════════════════════════
// 筛选（必须在 m_EntriesMutex 持有锁时调用）
// ════════════════════════════════════════════════════════════

// 判断单条是否命中当前关键词（区分大小写，严格子串匹配）
bool LogViewer::MatchesKeyword(const LogEntry& e) const {
    if (m_Keyword.empty()) return true;
    return e.text.find(m_Keyword) != std::string::npos;
}

// 全量重建：清空 stripe，从队头重筛全部。仅关键词变化 / 数据重置时调用。
void LogViewer::RebuildAll() {
    m_LogStripe->Clear();
    const size_t cnt = m_AllEntries.Size();
    for (size_t i = 0; i < cnt; i++) {
        const LogEntry& e = m_AllEntries[i];  // 相对索引，0 = 最旧
        if (MatchesKeyword(e))
            m_LogStripe->AddItem(e.text.c_str(), e.color);
    }
    m_RenderedSeq = m_AllEntries.TotalPushed();
    RequestRepaint();
}

// 增量喂入：把全局序号 [fromSeq, toSeq) 的新条目追加到 stripe（含 filter）。
// 锁内调用（Ticker 排他锁）。
void LogViewer::IncrementalAppend(uint64_t fromSeq, uint64_t toSeq) {
    if (toSeq <= fromSeq) return;
    for (uint64_t i = fromSeq; i < toSeq; i++) {
        const LogEntry& e = m_AllEntries.At(i);  // 按全局序号
        if (MatchesKeyword(e))
            m_LogStripe->AddItem(e.text.c_str(), e.color);
    }
    m_RenderedSeq = toSeq;
    RequestRepaint();
}

// ════════════════════════════════════════════════════════════
// 交互回调
// ════════════════════════════════════════════════════════════

void LogViewer::OnKeywordChanged(const std::string& text) {
    // 保留关键词原样（区分大小写，不做 tolower）
    m_Keyword = text;

    // 关键词变了 → 唯一一次全量重建
    std::lock_guard<std::shared_mutex> lock(m_EntriesMutex);
    RebuildAll();
}

// ════════════════════════════════════════════════════════════
// 绘制（主线程）
// ════════════════════════════════════════════════════════════

void LogViewer::OnPaint(Canvas& canvas) {
    LayoutChildren();

    canvas.FillRect(0, 0, get_width(), get_height(), 0xFF1E1E1E);

    // 共享锁：保护 m_LogStripe（Ticker 线程可能正增量写）以及绘制
    {
        std::shared_lock<std::shared_mutex> lock(m_EntriesMutex);
        Container::OnPaint(canvas);
    }
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
