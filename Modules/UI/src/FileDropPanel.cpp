#include "UI/Panel/FileDropPanel.h"

#include <algorithm>

namespace X_Y
{
    bool FileDropPanel::OpenFile(const XPath &file)
    {
        return OpenFiles(std::vector<XPath>{file});
    }

    bool FileDropPanel::OpenFiles(const std::vector<XPath> &files)
    {
        if (files.empty())
            return false;
        return OnOpenFiles(files);
    }

    void FileDropPanel::OnPaint(Canvas &canvas)
    {
        Panel::OnPaint(canvas);
        if (!m_FileDragHover || GetWidth() <= 0 || GetHeight() <= 0)
            return;

        const int horizontalBorder = std::min(3, GetHeight());
        const int verticalBorder = std::min(3, GetWidth());
        const int sideHeight = std::max(0, GetHeight() - horizontalBorder * 2);
        canvas.FillRect(0, 0, GetWidth(), horizontalBorder, 0xFF36A3FF);
        canvas.FillRect(0, GetHeight() - horizontalBorder, GetWidth(),
                        horizontalBorder, 0xFF36A3FF);
        canvas.FillRect(0, horizontalBorder, verticalBorder, sideHeight, 0xFF36A3FF);
        canvas.FillRect(GetWidth() - verticalBorder, horizontalBorder,
                        verticalBorder, sideHeight, 0xFF36A3FF);
    }

    void FileDropPanel::OnFileDragEnter(const std::vector<XPath> &files, int x, int y)
    {
        (void)x;
        (void)y;
        m_FileDragHover = !files.empty();
        RequestRepaint();
    }

    void FileDropPanel::OnFileDragOver(const std::vector<XPath> &files, int x, int y)
    {
        OnFileDragEnter(files, x, y);
    }

    void FileDropPanel::OnFileDragLeave()
    {
        m_FileDragHover = false;
        RequestRepaint();
    }

    void FileDropPanel::OnFileDrop(const std::vector<XPath> &files, int x, int y)
    {
        (void)x;
        (void)y;
        m_FileDragHover = false;
        OpenFiles(files);
        RequestRepaint();
    }
}
