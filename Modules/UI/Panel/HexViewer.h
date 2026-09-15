#pragma once

#include "UI/Panel/Panel.h"
#include "XCore/FilesSystem/FilesSystem.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace X_Y
{
    class Button;
    class Horizontal;
    class ScrollArea;

    class HexViewer final : public Panel
    {
    public:
        HexViewer();
        ~HexViewer() override;

        bool OpenFile(const XPath &path);
        void AddFile(const XPath &path);
        void ClearFiles();

        const XPath &GetActiveFile() const { return m_ActiveFile; }

    protected:
        void OnLayout() override;
        void OnPaint(Canvas &canvas) override;
        void OnFileDragEnter(const std::vector<XPath> &files, int x, int y) override;
        void OnFileDragOver(const std::vector<XPath> &files, int x, int y) override;
        void OnFileDragLeave() override;
        void OnFileDrop(const std::vector<XPath> &files, int x, int y) override;

    private:
        class BinaryContent;

        void ActivateFile(std::size_t index);
        void CloseFile(std::size_t index);
        void ProcessPendingClose();
        std::unique_ptr<Horizontal> m_FileBar;
        std::unique_ptr<ScrollArea> m_ScrollArea;
        std::unique_ptr<BinaryContent> m_Content;
        std::vector<std::unique_ptr<Button>> m_FileTabs;
        std::vector<XPath> m_Files;
        XPath m_ActiveFile;
        bool m_UpdatingSelection = false;
        bool m_FileDragHover = false;

        // ── 待关闭队列（延迟关闭）──
        // ⚠️ 为什么延迟：Button::OnInput 正在 Horizontal::Components 的遍历栈里，
        //    此刻把自己从容器删掉会 UAF —— 所以只登记，等下次 OnPaint 再结算。
        //
        // ⚠️ 为什么存【指针】而不是下标：
        //    下标在"登记"与"结算"之间会过期 —— 这段时间里若发生任何改动 m_Files 的
        //    操作（拖入新文件 OpenFile/AddFile、关掉别的 tab），下标指向的已经不是
        //    当初点 × 的那个文件了 ⇒ 会关错文件。
        //    指针稳定，结算时再反查当前下标。
        //
        // ⚠️ 为什么是【队列】而不是单个变量：两次点 × 之间若没发生重绘，
        //    单变量会被后一次覆盖 ⇒ 前一个关闭请求丢失（点了没反应）。
        std::vector<Button *> m_PendingClose;

        static constexpr int kFileBarHeight = 28;
    };
}