#pragma once
#include"Buffer/include/Buffer.h"
#include<filesystem>
#include <regex>
namespace X_Y {
    using File = std::filesystem::path;

    class FilesSystem {
    public:
        static bool FileExists(const File& filepath)
        {
            return std::filesystem::exists(filepath);
        }
        static Buffer ReadFileBinary(const File& filepath);
        static bool WriteFileBinary(const File& filepath, const Buffer& buffer);

#ifdef XY_PLATFORM_WINDOWS
        static std::string OpenFileDialog(const char* filter = "All Files (*.*)\0*.*\0");
        static std::string SaveFileDialog(const char* filter = "All Files (*.*)\0*.*\0");
#endif
    };

    /// @brief 路径对象，类似 Python 的 pathlib.Path
    ///        每个 XPath 代表一个文件或目录路径，操作作用于自身
    class XPath {
    public:
        XPath() = default;
        XPath(const File& path) : m_Path(path) {}

        // ── 路径访问 ──
        const File& Path() const { return m_Path; }
        void SetPath(const File& path) { m_Path = path; }
        bool Empty() const { return m_Path.empty(); }
        //通用操作
		bool IsAbsolute() const { return m_Path.is_absolute(); }
		bool IsRelative() const { return m_Path.is_relative(); }
        bool Exists() const { return FilesSystem::FileExists(m_Path); }
		bool Remove() const { return std::filesystem::remove(m_Path); }
		bool Rename(const File& newPath) { std::error_code ec; std::filesystem::rename(m_Path, newPath, ec); return !ec; }
        XPath GetParent() const { return XPath(m_Path.parent_path()); }
		bool IsPathEqual(const XPath& other) const { return std::filesystem::equivalent(m_Path, other.m_Path); }
        std::string getName() const { return m_Path.filename().string(); }
        bool MatchName(const std::string& pattern)const {
			if (pattern.empty()) return false;
            std::string name = getName();
            std::regex reg(pattern);
            return std::regex_match(name, reg);
        }
        
        // ── 文件操作（对自身路径） ──
		bool CreateFile() const { std::ofstream ofs(m_Path); return ofs.good(); }
		bool IsFile() const { return std::filesystem::is_regular_file(m_Path); }
        Buffer ReadBinary() const { return FilesSystem::ReadFileBinary(m_Path); }
        bool WriteBinary(const Buffer& buf) const { return FilesSystem::WriteFileBinary(m_Path, buf); }
		bool MatchExtension(const std::string& extension) const { return std::filesystem::path(m_Path).extension() == extension; }
		bool CopyTo(const File& dest) const { std::error_code ec; std::filesystem::copy_file(m_Path, dest, std::filesystem::copy_options::overwrite_existing, ec); return !ec; }
        //——文件夹操作——
		bool CreateDirectory() { std::error_code ec; std::filesystem::create_directories(m_Path, ec); return !ec; }
        bool IsDirectory() const { return std::filesystem::is_directory(m_Path); }
		std::vector<XPath> ListDirectory() const {
			std::vector<XPath> entries;
			if (IsDirectory()) {
				for (const auto& entry : std::filesystem::directory_iterator(m_Path)) {
					entries.emplace_back(entry.path());
				}
			}
			return entries;
		}
		std::vector<XPath> ListMatching(const std::string& pattern) const {
			std::vector<XPath> matchingEntries;
			if (IsDirectory()) {
				for (const auto& entry : std::filesystem::directory_iterator(m_Path)) {
					XPath xpath(entry.path());
					if (xpath.MatchName(pattern)) {
						matchingEntries.push_back(xpath);
					}
				}
			}
			return matchingEntries;
		}
		bool RemoveAll() { std::error_code ec; std::filesystem::remove_all(m_Path, ec); return !ec; }
		bool CopyDirectoryTo(const File& dest) const {
			std::error_code ec;
			std::filesystem::copy(m_Path, dest, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
			return !ec;
		}
        // ── 路径拼接 ──
		XPath& operator=(const File& sub) { m_Path = sub; return *this; }
        XPath& operator/=(const File& sub) { m_Path /= sub; return *this; }
        XPath& operator+=(const File& sub) { m_Path /= sub; return *this; }
        XPath operator/(const File& sub) const { XPath r = *this; r.m_Path /= sub; return r; }
        XPath operator+(const File& sub) const { XPath r = *this; r.m_Path /= sub; return r; }

        // ── 自动转 File（std::filesystem::path），方便传参 ──
        operator const File&() const { return m_Path; }

    private:
        File m_Path;
    };
}
