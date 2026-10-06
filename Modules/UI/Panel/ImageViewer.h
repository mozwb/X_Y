#pragma once

#include "UI/Panel/FileDropPanel.h"

namespace X_Y
{
    class ImageView;
    class Label;

    class ImageViewer final : public FileDropPanel
    {
    public:
        ImageViewer();

        bool OpenFile(const XPath &path);

    protected:
        void OnLayout() override;
        void OnPaint(Canvas &canvas) override;
        bool OnOpenFiles(const std::vector<XPath> &files) override;

    private:
        ImageView *m_ImageView = nullptr;
        Label *m_Status = nullptr;
    };
}
