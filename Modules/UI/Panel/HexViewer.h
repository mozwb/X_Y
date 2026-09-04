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
        std::size_t m_PendingClose = static_cast<std::size_t>(-1);

        static constexpr int kFileBarHeight = 28;
    };
}