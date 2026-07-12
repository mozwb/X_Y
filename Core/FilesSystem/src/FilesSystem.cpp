#include"FilesSystem/include/FilesSystem.h"
#include<fstream>
#ifdef XY_PLATFORM_WINDOWS
#include<Windows.h>
#include<commdlg.h>
#endif
namespace X_Y {

	Buffer FilesSystem::ReadFileBinary(const File& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

		if (!stream)
		{
			// Failed to open the file
			return {};
		}


		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint64_t size = end - stream.tellg();

		if (size == 0)
		{
			// File is empty
			return {};
		}

		Buffer buffer(size);
		stream.read(buffer.As<char>(), size);
		buffer.Size = size;
		stream.close();
		return buffer;
	}

	bool FilesSystem::WriteFileBinary(const File& filepath, const Buffer& buffer)
	{
		std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);

		if (!stream)
		{
			// Failed to open the file for writing
			return false;
		}

		stream.write(reinterpret_cast<const char*>(buffer.Data), buffer.Size);
		stream.close();
		return true;
	}

#ifdef XY_PLATFORM_WINDOWS

	std::string FilesSystem::OpenFileDialog(const char* filter)
	{
		OPENFILENAMEA ofn = {};
		CHAR szFile[260] = {};
		CHAR currentDir[256] = {};

		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = nullptr;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;

		if (GetOpenFileNameA(&ofn))
			return std::string(ofn.lpstrFile);

		return std::string();
	}

	std::string FilesSystem::SaveFileDialog(const char* filter)
	{
		OPENFILENAMEA ofn = {};
		CHAR szFile[260] = {};
		CHAR currentDir[256] = {};

		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = nullptr;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		ofn.lpstrDefExt = strchr(filter, '\0') + 1;

		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;

		if (GetSaveFileNameA(&ofn))
			return std::string(ofn.lpstrFile);

		return std::string();
	}

#endif

}