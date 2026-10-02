#pragma once

#include "../Memory/Buffer.h"
#include <filesystem>
#include <fstream>
#include <optional>

namespace X_Y
{
    enum class StreamReadStatus
    {
        Data,
        EndOfStream,
        Error
    };

    struct StreamReadResult
    {
        uint64_t BytesRead = 0;
        StreamReadStatus Status = StreamReadStatus::EndOfStream;
    };

    class Stream
    {
    public:
        virtual ~Stream() = default;

        virtual StreamReadResult Read(void *destination, uint64_t capacity) = 0;
        virtual bool Seek(uint64_t position) = 0;
        virtual uint64_t Tell() const = 0;
        virtual std::optional<uint64_t> Size() const = 0;
        virtual bool IsOpen() const = 0;
        virtual void Close() = 0;
    };

    class FileStream final : public Stream
    {
    public:
        FileStream() = default;
        explicit FileStream(const std::filesystem::path &path) { Open(path); }
        ~FileStream() override { Close(); }

        bool Open(const std::filesystem::path &path);
        StreamReadResult Read(void *destination, uint64_t capacity) override;
        bool Seek(uint64_t position) override;
        uint64_t Tell() const override { return m_Position; }
        std::optional<uint64_t> Size() const override { return m_Size; }
        bool IsOpen() const override { return m_File.is_open(); }
        void Close() override;

    private:
        std::ifstream m_File;
        std::optional<uint64_t> m_Size;
        uint64_t m_Position = 0;
    };

    class BufferStream final : public Stream
    {
    public:
        BufferStream() = default;
        explicit BufferStream(BufferView source) { Open(source); }

        void Open(BufferView source);
        StreamReadResult Read(void *destination, uint64_t capacity) override;
        bool Seek(uint64_t position) override;
        uint64_t Tell() const override { return m_Position; }
        std::optional<uint64_t> Size() const override { return m_Source.Size; }
        bool IsOpen() const override { return m_IsOpen; }
        void Close() override;

    private:
        BufferView m_Source;
        uint64_t m_Position = 0;
        bool m_IsOpen = false;
    };
}