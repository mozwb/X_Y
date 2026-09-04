#include "../panel/HexViewer.h"
#include "UI/Component/Button.h"
#include "UI/Component/ScrollArea.h"
#include "UI/Component/horizontal.h"
#include "Widget/FontLibrary.h"
#include "XCore/Memory/Buffer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>

namespace X_Y
{
    class HexViewer::BinaryContent final : public Component
    {
    public:
        bool OpenFile(const XPath &path)
        {
            if (path.Empty() || !path.IsFile())
                return false;

            Buffer data = path.ReadBinary();
            if (data.Size > 0 && !data.Data)
                return false;

            m_File = path;
            m_Data = std::move(data);
            m_ViewportOffset = 0;
            const uint64_t rowCount = (m_Data.Size + kBytesPerRow - 1) / kBytesPerRow;
            const int contentHeight = kHeaderHeight + static_cast<int>(rowCount) * kRowHeight;
            SetRect(GetX(), GetY(), GetWidth(), std::max(kHeaderHeight, contentHeight));
            return true;
        }

        void ClearFile()
        {
            m_File = XPath();
            m_Data.Release();
            m_ViewportOffset = 0;
            SetRect(GetX(), GetY(), GetWidth(), kHeaderHeight);
        }

        int GetScrollStep() const override { return kRowHeight; }

        void SetViewport(int scrollOffset, int viewHeight) override
        {
            m_ViewportOffset = std::max(0, scrollOffset);
            m_ViewportHeight = std::max(0, viewHeight);
        }

        void OnPaint(Canvas &canvas) override
        {
            const int x = GetX();
            const int y = GetY();
            const int width = GetWidth();
            const int firstRow = std::max(0, m_ViewportOffset / kRowHeight);
            const uint64_t rowCount = (m_Data.Size + kBytesPerRow - 1) / kBytesPerRow;
            const int lastRow = std::min<uint64_t>(rowCount,
                                                   firstRow + m_ViewportHeight / kRowHeight + 2);

            const Font &font = FontLibrary::Instance().GetDefault();
            const int addressWidth = font.MeasureText("0000000000000000");
            const int hexX = x + kLeftPadding + addressWidth + kColumnGap;
            canvas.FillRect(x, y, width, kHeaderHeight, 0xFF202832);
            canvas.DrawText(font, x + kLeftPadding, y + 4, "Offset", 0xFF9DAAB8);
            canvas.DrawText(font, hexX, y + 4, "Hexadecimal", 0xFF9DAAB8);

            for (int row = firstRow; row < lastRow; ++row)
            {
                const uint64_t offset = static_cast<uint64_t>(row) * kBytesPerRow;
                if (offset >= m_Data.Size)
                    break;

                char address[32] = {};
                char hex[3 * kBytesPerRow + 1] = {};
                FormatOffset(address, offset);
                for (int column = 0; column < kBytesPerRow; ++column)
                {
                    const uint64_t index = offset + column;
                    if (index >= m_Data.Size)
                    {
                        hex[column * 3] = ' ';
                        hex[column * 3 + 1] = ' ';
                        hex[column * 3 + 2] = ' ';
                        continue;
                    }

                    const unsigned char value = m_Data[index];
                    const char *digits = "0123456789ABCDEF";
                    hex[column * 3] = digits[value >> 4];
                    hex[column * 3 + 1] = digits[value & 0x0F];
                    hex[column * 3 + 2] = ' ';
                }
                hex[3 * kBytesPerRow] = '\0';

                const int rowY = y + kHeaderHeight + row * kRowHeight;
                if ((row & 1) == 0)
                    canvas.FillRect(x, rowY, width, kRowHeight, 0xFF171D24);
                char line[sizeof(address) + 2 + sizeof(hex)] = {};
                std::memcpy(line, address, 16);
                line[16] = ' ';
                line[17] = ' ';
                std::memcpy(line + 18, hex, sizeof(hex));
                canvas.DrawText(font, x + kLeftPadding, rowY + 2,
                                line, 0xFFD7DEE5);
            }
        }

    private:
        static void FormatOffset(char (&buffer)[32], uint64_t offset)
        {
            static constexpr char digits[] = "0123456789ABCDEF";
            for (int i = 0; i < 16; ++i)
                buffer[i] = digits[(offset >> ((15 - i) * 4)) & 0x0F];
            buffer[16] = '\0';
        }

        static constexpr int kBytesPerRow = 16;
        static constexpr int kRowHeight = 20;
        static constexpr int kHeaderHeight = 24;
        static constexpr int kLeftPadding = 8;
        static constexpr int kColumnGap = 16;

        XPath m_File;
        Buffer m_Data;
        int m_ViewportOffset = 0;
        int m_ViewportHeight = 0;
    };

    HexViewer::HexViewer()
        : Panel(),
          m_FileBar(std::make_unique<Horizontal>()),
          m_ScrollArea(std::make_unique<ScrollArea>()),
          m_Content(std::make_unique<BinaryContent>())
    {
        m_ScrollArea->SetContent(m_Content.get());
        m_FileBar->OnSelected = [this](std::size_t index, Component *)
        {
            if (!m_UpdatingSelection)
                ActivateFile(index);
        };
        Panel::AddComponent(m_FileBar.get());
        Panel::AddComponent(m_ScrollArea.get());
    }

    HexViewer::~HexViewer() = default;

    bool HexViewer::OpenFile(const XPath &path)
    {
        if (path.Empty() || !path.IsFile())
            return false;

        auto it = std::find_if(m_Files.begin(), m_Files.end(),
                               [&path](const XPath &file)
                               { return file.IsPathEqual(path); });
        if (it == m_Files.end())
        {
            AddFile(path);
            it = std::prev(m_Files.end());
        }

        ActivateFile(static_cast<std::size_t>(it - m_Files.begin()));
        return !m_ActiveFile.Empty();
    }

    void HexViewer::AddFile(const XPath &path)
    {
        if (path.Empty() || !path.IsFile())
            return;

        m_Files.push_back(path);
        const std::size_t index = m_Files.size() - 1;
        auto tab = std::make_unique<Button>();
        tab->SetText(path.getName().c_str());
        tab->SetRect(0, 0, 180, kFileBarHeight);
        tab->SetRounded(true);
        tab->SetCloseable(true);
        tab->SetBgColor(0xFF29313A);
        tab->SetHoverColor(0xFF3B4D5E);
        tab->SetTextColor(0xFFE6EDF3);
        tab->SetOnClose([this, index]()
                        { m_PendingClose = index; });
        m_FileBar->AddComponent(tab.get());
        m_FileTabs.push_back(std::move(tab));

        if (m_Files.size() == 1)
            ActivateFile(0);
        RequestRepaint();
    }

    void HexViewer::ClearFiles()
    {
        m_Files.clear();
        m_FileTabs.clear();
        m_FileBar->Clear();
        m_Content->ClearFile();
        m_ActiveFile = XPath();
        RequestRepaint();
    }

    void HexViewer::ActivateFile(std::size_t index)
    {
        if (index >= m_Files.size() || !m_Content->OpenFile(m_Files[index]))
            return;

        m_UpdatingSelection = true;
        m_FileBar->Select(index);
        m_UpdatingSelection = false;
        m_ActiveFile = m_Files[index];
        for (std::size_t i = 0; i < m_FileTabs.size(); ++i)
            m_FileTabs[i]->SetBgColor(i == index ? 0xFF36506A : 0xFF29313A);
        RequestRepaint();
    }

    void HexViewer::CloseFile(std::size_t index)
    {
        if (index >= m_Files.size())
            return;

        m_FileBar->RemoveComponent(m_FileTabs[index].get());
        m_FileTabs.erase(m_FileTabs.begin() + index);
        m_Files.erase(m_Files.begin() + index);

        for (std::size_t i = 0; i < m_FileTabs.size(); ++i)
        {
            m_FileTabs[i]->SetOnClose([this, i]()
                                      { CloseFile(i); });
        }

        if (m_Files.empty())
        {
            m_Content->ClearFile();
            m_ActiveFile = XPath();
        }
        else
        {
            const std::size_t nextIndex = std::min(index, m_Files.size() - 1);
            ActivateFile(nextIndex);
        }
        RequestRepaint();
    }

    void HexViewer::ProcessPendingClose()
    {
        if (m_PendingClose == static_cast<std::size_t>(-1))
            return;

        const std::size_t index = m_PendingClose;
        m_PendingClose = static_cast<std::size_t>(-1);
        CloseFile(index);
    }

    void HexViewer::OnLayout()
    {
        const int width = GetWidth();
        const int height = GetHeight();
        const int barHeight = std::min(kFileBarHeight, std::max(0, height));
        m_FileBar->SetRect(0, 0, width, barHeight);
        m_ScrollArea->SetRect(0, barHeight, width, std::max(0, height - barHeight));
    }

    void HexViewer::OnPaint(Canvas &canvas)
    {
        ProcessPendingClose();
        canvas.FillRect(GetX(), GetY(), GetWidth(), GetHeight(), 0xFF11151A);
        Panel::OnPaint(canvas);
        if (m_FileDragHover)
        {
            canvas.FillRect(GetX(), GetY(), GetWidth(), 3, 0xFF36A3FF);
            canvas.FillRect(GetX(), GetY() + GetHeight() - 3,
                            GetWidth(), 3, 0xFF36A3FF);
        }
    }

    void HexViewer::OnFileDragEnter(const std::vector<XPath> &files, int x, int y)
    {
        (void)x;
        (void)y;
        m_FileDragHover = !files.empty();
        RequestRepaint();
    }

    void HexViewer::OnFileDragOver(const std::vector<XPath> &files, int x, int y)
    {
        OnFileDragEnter(files, x, y);
    }

    void HexViewer::OnFileDragLeave()
    {
        m_FileDragHover = false;
        RequestRepaint();
    }

    void HexViewer::OnFileDrop(const std::vector<XPath> &files, int x, int y)
    {
        (void)x;
        (void)y;
        m_FileDragHover = false;
        for (const XPath &file : files)
            OpenFile(file);
        RequestRepaint();
    }
}