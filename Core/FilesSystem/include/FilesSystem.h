#pragma once
#include"Buffer/include/Buffer.h"
#include<filesystem>
namespace X_Y {
	class FilesSystem {
	public:
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
		static bool WriteFileBinary(const std::filesystem::path& filepath, const Buffer& buffer);

#ifdef XY_PLATFORM_WINDOWS
		// 系统文件对话框，返回选中的文件路径（空字符串表示取消）
		// filter 格式："All Files (*.*)\0*.*\0" 或 "Image Files (*.png)\0*.png\0"
		static std::string OpenFileDialog(const char* filter = "All Files (*.*)\0*.*\0");
		static std::string SaveFileDialog(const char* filter = "All Files (*.*)\0*.*\0");
#endif
	};
}
