#include "XCore/Decoder/Blueprint/Image.h"

extern "C"
{
#include "__pngdec.h"
}

#include "XCore/FilesSystem/FilesSystem.h"
#include <cstdlib>

namespace X_Y
{
	Image::Image(const File &filepath)
	{
		Decode(filepath);
	}

	/* ────────────────────────────────────────────────
	 *  构造函数：从已读取的二进制数据
	 * ──────────────────────────────────────────────── */
	Image::Image(const Buffer &fileData)
	{
		Decode(fileData);
	}

	void Image::Decode(const File &filepath)
	{
		m_Pixels.Release();
		m_Width = 0;
		m_Height = 0;
		m_Channels = 0;
		m_Loaded = false;
		m_Error = ImageError::None;

		Buffer fileData = FilesSystem::ReadFileBinary(filepath);
		if (!fileData)
		{
			m_Error = ImageError::FileNotFound;
			return;
		}
		Decode(fileData);
	}

	void Image::Decode(const Buffer &fileData)
	{
		m_Pixels.Release();
		m_Width = 0;
		m_Height = 0;
		m_Channels = 0;
		m_Loaded = false;
		m_Error = ImageError::None;

		if (!fileData || fileData.Size == 0)
		{
			m_Error = ImageError::InvalidData;
			return;
		}

		unsigned char *rgba = nullptr;
		unsigned int width = 0, height = 0, channels = 0;
		const int result = png_decode(
			fileData.Data,
			static_cast<size_t>(fileData.Size),
			&rgba,
			&width, &height, &channels);

		if (result != PNG_OK)
		{
			m_Error = ImageError::DecodeFailed;
			return;
		}

		const uint64_t pixelSize = static_cast<uint64_t>(width) * height * channels;
		m_Pixels.Overwrite(rgba, pixelSize);
		std::free(rgba);
		if (m_Pixels.Size != pixelSize)
		{
			m_Error = ImageError::DecodeFailed;
			return;
		}

		m_Width = width;
		m_Height = height;
		m_Channels = channels;
		m_Loaded = true;
	}

	Image::Image(Image &&other) noexcept
		: m_Pixels(std::move(other.m_Pixels)),
		  m_Width(other.m_Width),
		  m_Height(other.m_Height),
		  m_Channels(other.m_Channels),
		  m_Loaded(other.m_Loaded),
		  m_Error(other.m_Error)
	{
		other.m_Width = 0;
		other.m_Height = 0;
		other.m_Channels = 0;
		other.m_Loaded = false;
		other.m_Error = ImageError::None;
	}

	Image &Image::operator=(Image &&other) noexcept
	{
		if (this != &other)
		{
			m_Pixels = std::move(other.m_Pixels);
			m_Width = other.m_Width;
			m_Height = other.m_Height;
			m_Channels = other.m_Channels;
			m_Loaded = other.m_Loaded;
			m_Error = other.m_Error;

			other.m_Width = 0;
			other.m_Height = 0;
			other.m_Channels = 0;
			other.m_Loaded = false;
			other.m_Error = ImageError::None;
		}
		return *this;
	}

} // namespace X_Y
