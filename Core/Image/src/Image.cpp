#include "Image.h"

extern "C" {
#include "__pngdec.h"
}

#include "Log/include/XYlog.h"
#include "FilesSystem/include/FilesSystem.h"

namespace X_Y {
	Image::Image(const File& filepath)
	{
		Buffer fileData = FilesSystem::ReadFileBinary(filepath);
		if (!fileData)
		{
			m_Error = ImageError::FileNotFound;
			XFATAL("Image: failed to read file: {0}", filepath.string());
			return;
		}
		DecodeFromBuffer(fileData);
	}

	/* ────────────────────────────────────────────────
	 *  构造函数：从已读取的二进制数据
	 * ──────────────────────────────────────────────── */
	Image::Image(const Buffer& fileData)
	{
		DecodeFromBuffer(fileData);
	}

	/* ────────────────────────────────────────────────
	 *  移动构造 / 移动赋值
	 * ──────────────────────────────────────────────── */
	Image::Image(Image&& other) noexcept
		: m_Pixels(std::move(other.m_Pixels))
		, m_Width(other.m_Width)
		, m_Height(other.m_Height)
		, m_Channels(other.m_Channels)
		, m_Loaded(other.m_Loaded)
		, m_Error(other.m_Error)
	{
		other.m_Width = 0;
		other.m_Height = 0;
		other.m_Channels = 0;
		other.m_Loaded = false;
		other.m_Error = ImageError::None;
	}

	Image& Image::operator=(Image&& other) noexcept
	{
		if (this != &other)
		{
			m_Pixels   = std::move(other.m_Pixels);
			m_Width    = other.m_Width;
			m_Height   = other.m_Height;
			m_Channels = other.m_Channels;
			m_Loaded   = other.m_Loaded;
			m_Error    = other.m_Error;

			other.m_Width = 0;
			other.m_Height = 0;
			other.m_Channels = 0;
			other.m_Loaded = false;
			other.m_Error = ImageError::None;
		}
		return *this;
	}

	/* ────────────────────────────────────────────────
	 *  显式拷贝
	 * ──────────────────────────────────────────────── */
	Image Image::Copy() const
	{
		Image result;
		result.m_Pixels   = m_Pixels.Copy();
		result.m_Width    = m_Width;
		result.m_Height   = m_Height;
		result.m_Channels = m_Channels;
		result.m_Loaded   = m_Loaded;
		result.m_Error    = m_Error;
		return result;
	}

	/* ────────────────────────────────────────────────
	 *  内部解码：调用 __pngdec 的纯 C 解码器
	 * ──────────────────────────────────────────────── */
	void Image::DecodeFromBuffer(const Buffer& fileData)
	{
		if (!fileData || fileData.Size == 0)
		{
			m_Error = ImageError::InvalidData;
			return;
		}

		unsigned char* rgba = nullptr;
		unsigned int w = 0, h = 0, ch = 0;

		int result = png_decode(
			fileData.Data,
			(size_t)fileData.Size,
			&rgba,
			&w, &h, &ch
		);

		if (result != PNG_OK)
		{
			m_Error = ImageError::DecodeFailed;
			XFATAL("Image: png_decode failed with error code {0}", result);
			return;
		}

		/* ── 将解码结果移入 Buffer，由 Buffer 管理生命周期 ── */
		uint64_t pixelSize = (uint64_t)w * h * ch;
		m_Pixels.Data     = rgba;
		m_Pixels.Size     = pixelSize;
		m_Pixels.Capacity = pixelSize;
		m_Pixels.bFreeInstead = true;  // malloc 来的，用 free() 释放

		m_Width    = w;
		m_Height   = h;
		m_Channels = ch;
		m_Loaded   = true;
		m_Error    = ImageError::None;

		XDEBUG("Image: loaded {0}x{1}x{2} ({3} bytes)",
			w, h, ch, pixelSize);
	}

} // namespace X_Y
