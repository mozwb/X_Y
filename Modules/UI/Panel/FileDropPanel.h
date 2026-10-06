#pragma once

#include "Panel.h"

#include <vector>

namespace X_Y
{
    class FileDropPanel : public Panel
    {
    public:
        // Route paths from any source through the same handler used for file drops.
        bool OpenFile(const XPath &file);
        bool OpenFiles(const std::vector<XPath> &files);

        void OnPaint(Canvas &canvas) override;
        void OnFileDragEnter(const std::vector<XPath> &files, int x, int y) final;
        void OnFileDragOver(const std::vector<XPath> &files, int x, int y) final;
        void OnFileDragLeave() final;
        void OnFileDrop(const std::vector<XPath> &files, int x, int y) final;

    protected:
        virtual bool OnOpenFiles(const std::vector<XPath> &files) = 0;

    private:
        bool m_FileDragHover = false;
    };
}
