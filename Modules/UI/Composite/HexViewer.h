#pragma once

#include "UI/Container/Container.h"
#include "XCore/FilesSystem/FilesSystem.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace X_Y
{
    class Button;
    class Horizontal;
    class ScrollArea;

    class HexViewer final : public Container
    {
    public:
        explicit HexViewer(XWidget *parent = nullptr);
        ~HexViewer() override;

        bool OpenFile(const XPath &path);
        void AddFile(const XPath &path);
        void ClearFiles();

        const XPath &GetActiveFile() const { return m_ActiveFile; }

    protected:
        void OnPaint(Canvas *canvas) override;
        void OnFileDrop(const std::vector<XPath> &files, int x, int y) override;

    private:
        class BinaryContent;

        void ActivateFile(std::size_t index);
        void CloseFile(std::size_t index);
        void ProcessPendingClose();
        void LayoutChildren();

        std::unique_ptr<Horizontal> m_FileBar;
        std::unique_ptr<ScrollArea> m_ScrollArea;
        std::unique_ptr<BinaryContent> m_Content;
        std::vector<std::unique_ptr<Button>> m_FileTabs;
        std::vector<XPath> m_Files;
        XPath m_ActiveFile;
        bool m_UpdatingSelection = false;
        std::size_t m_PendingClose = static_cast<std::size_t>(-1);

        static constexpr int kFileBarHeight = 28;
    };
}