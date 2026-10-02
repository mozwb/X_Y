#include "XCore/Stream/Stream.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace X_Y
{
    bool FileStream::Open(const std::filesystem::path &path)
    {
        Close();
        m_File.open(path, std::ios::binary);
        if (!m_File)
            return false;

        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (!error)
            m_Size = size;
        return true;
    }

    StreamReadResult FileStream::Read(void *destination, uint64_t capacity)
    {
        if (!IsOpen() || (capacity > 0 && destination == nullptr))
            return {0, StreamReadStatus::Error};
        if (capacity == 0)
            return {0, StreamReadStatus::Data};

        const auto maxRead = static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max());
        const auto requested = std::min(capacity, maxRead);
        m_File.read(static_cast<char *>(destination), static_cast<std::streamsize>(requested));
        const auto bytesRead = static_cast<uint64_t>(m_File.gcount());
        m_Position += bytesRead;

        if (bytesRead > 0)
            return {bytesRead, StreamReadStatus::Data};
        if (m_File.eof())
            return {0, StreamReadStatus::EndOfStream};
        return {0, StreamReadStatus::Error};
    }

    bool FileStream::Seek(uint64_t position)
    {
        if (!IsOpen() || position > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max()))
            return false;

        m_File.clear();
        m_File.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        if (!m_File)
            return false;

        m_Position = position;
        return true;
    }

    void FileStream::Close()
    {
        if (m_File.is_open())
            m_File.close();
        m_File.clear();
        m_Size.reset();
        m_Position = 0;
    }

    void BufferStream::Open(BufferView source)
    {
        m_Source = source;
        m_Position = 0;
        m_IsOpen = true;
    }

    StreamReadResult BufferStream::Read(void *destination, uint64_t capacity)
    {
        if (!IsOpen() || (capacity > 0 && destination == nullptr))
            return {0, StreamReadStatus::Error};
        if (capacity == 0)
            return {0, StreamReadStatus::Data};
        if (m_Position >= m_Source.Size)
            return {0, StreamReadStatus::EndOfStream};

        const auto bytesRead = std::min(capacity, m_Source.Size - m_Position);
        std::memcpy(destination, m_Source.Data + m_Position, static_cast<size_t>(bytesRead));
        m_Position += bytesRead;
        return {bytesRead, StreamReadStatus::Data};
    }

    bool BufferStream::Seek(uint64_t position)
    {
        if (!IsOpen() || position > m_Source.Size)
            return false;
        m_Position = position;
        return true;
    }

    void BufferStream::Close()
    {
        m_Source = {};
        m_Position = 0;
        m_IsOpen = false;
    }
}