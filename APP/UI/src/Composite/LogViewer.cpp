#include "Composite/LogViewer.h"
#include "DataStore/include/DataStore.h"
#include "Timer/include/Timer.h"
#include "Widget/include/BaseWin.h"
#include <algorithm>

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

    m_TagBar = std::make_unique<TagBar>();
    // 点某个 tag 的 × → 移除该筛选关键词并重筛
    m_TagBar->OnTagRemove = [this](const std::string& tag) {
        OnTagRemoved(tag);
    };

    m_KeywordInput = std::make_unique<TextInput>();
    m_KeywordInput->SetPlaceholder("输入关键词，回车添加到筛选");
    // 输入框回车 → 添加为筛选规则
    m_KeywordInput->OnEnter = [this]() {
        OnEnterKeyword();
    };

    m_LogStripe = std::make_unique<LogStripe>();

    m_ScrollArea = std::make_unique<ScrollArea>();
    m_ScrollArea->SetContent(m_LogStripe.get());

    AddComponent(m_TagBar.get());
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

// 拆一个筛选 tag 的 AND 子项：按 "&&" 分割，去除空白。
// 示例："error && info" → {"error","info"}；"error" → {"error"}
static std::vector<std::string> SplitAndParts(const std::string& tag) {
    std::vector<std::string> parts;
    std::string cur;
    const size_t n = tag.size();
    size_t i = 0;
    // 逐段找 "&&"
    while (i < n) {
        size_t found = tag.find("&&", i);
        if (found == std::string::npos) {
            cur = tag.substr(i);
            i = n;
        } else {
            cur = tag.substr(i, found - i);
            i = found + 2;
        }
        // 去首尾空白（空格/Tab），空段忽略
        size_t b = cur.find_first_not_of(" 	");
        size_t e2 = cur.find_last_not_of(" 	");
        if (b != std::string::npos && e2 >= b)
            parts.push_back(cur.substr(b, e2 - b + 1));
        cur.clear();
    }
    return parts;
}

// 判断单条是否命中当前筛选：
//   tag 之间 OR（命中任一 tag 即通过）；单个 tag 内用 "&&" 表达 AND（需同时命中所有子项）
//   （区分大小写，严格子串匹配）
bool LogViewer::MatchesKeyword(const LogEntry& e) const {
    if (m_Keywords.empty()) return true;
    for (const auto& kw : m_Keywords) {
        std::vector<std::string> parts = SplitAndParts(kw);
        if (parts.empty()) continue;
        bool all = true;
        for (const auto& part : parts) {
            if (part.empty() || e.text.find(part) == std::string::npos) {
                all = false;
                break;
            }
        }
        if (all)
            return true;
    }
    return false;
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

// 输入框回车：把当前输入作为一条筛选规则添加，并清空输入框
void LogViewer::OnEnterKeyword() {
    std::string kw = m_KeywordInput->GetText();
    // 空关键词忽略
    if (kw.empty()) return;

    std::lock_guard<std::shared_mutex> lock(m_EntriesMutex);
    // 去重：已存在同样的关键词则不重复添加
    for (const auto& k : m_Keywords)
        if (k == kw) return;

    m_Keywords.push_back(kw);
    m_TagBar->AddTag(kw);
    RebuildAll();

    m_KeywordInput->SetText("");
}

// 删除某条筛选规则并重筛
void LogViewer::OnTagRemoved(const std::string& tag) {
    std::lock_guard<std::shared_mutex> lock(m_EntriesMutex);
    auto it = std::find(m_Keywords.begin(), m_Keywords.end(), tag);
    if (it != m_Keywords.end())
        m_Keywords.erase(it);
    // tag 也要从 TagBar 上移除，否则视觉上删不掉
    m_TagBar->RemoveTag(tag);
    RebuildAll();
}

// ════════════════════════════════════════════════════════════
// 绘制（主线程）
// ════════════════════════════════════════════════════════════

void LogViewer::OnPaint(Canvas& canvas) {
    // 先让 TagBar 用真实 Canvas 量高（测字宽 + 自动换行），布局时才能用当下正确的高度，
    // 避免日志区一帧错位（TagBar 高度随 tag 增减/换行实时变化）
    m_TagBar->SetRect(0, 0, get_width(), m_TagBar->GetHeight());
    m_TagBar->Measure(canvas);
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
    const int gap = 2;

    // 顶栏：TagBar（筛选规则条，高度已在 OnPaint 用 canvas 量好）
    m_TagBar->SetRect(0, 0, w, m_TagBar->GetHeight());

    // 输入框：TagBar 之下
    int inputY = m_TagBar->GetHeight() + gap;
    m_KeywordInput->SetRect(0, inputY, w, inputH);

    // 日志区：占满剩余
    int panelY = inputY + inputH + gap;
    int panelH = h - panelY;
    if (panelH > 0)
        m_ScrollArea->SetRect(0, panelY, w, panelH);
}

}
// namespace X_Y
