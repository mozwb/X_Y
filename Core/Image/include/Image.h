#pragma once

#include "Buffer/include/Buffer.h"

namespace X_Y {

	/**
	 * Image — 引擎图片抽象层
	 *
	 * 封装 PNG 解码（及其他图片格式），提供像素数据、元信息。
	 * 设计目标：① 内存加载 ② 不暴露底层解码器 ③ 可直接喂给 Texture2D 创建 OpenGL 纹理。
	 *
	 * 典型用法：
	 *   Image img("textures/foo.png");
	 *   // 或
	 *   Buffer fileData = FilesSystem::ReadFileBinary("textures/foo.png");
	 *   Image img(fileData);
	 */

	enum class ImageError
	{
		None = 0,
		InvalidData,
		UnsupportedFormat,
		DecodeFailed,
		FileNotFound
	};

	class Image
	{
	public:
		/* ── 从原始文件数据构造 ── */
		explicit Image(const Buffer& fileData);

		/* ── 从文件路径构造 ── */
		explicit Image(const std::filesystem::path& filepath);

		Image() = default;
		Image(Image&& other) noexcept;
		Image& operator=(Image&& other) noexcept;

		/* 禁止拷贝（像素数据可能很大，按需显式 Copy()） */
		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;

		~Image() = default;

		/* ── 元信息 ── */
		uint32_t GetWidth()  const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		uint32_t GetChannels() const { return m_Channels; }

		/* 像素数据 — Buffer 不拥有该内存，仅作只读视图 */
		BufferView GetPixels() const { return BufferView(m_Pixels); }
		const Buffer& GetPixelBuffer() const { return m_Pixels; }

		/* 是否成功加载 */
		bool IsLoaded() const { return m_Loaded; }

		/* 最后一次加载的错误信息 */
		ImageError GetError() const { return m_Error; }

		/* ── 工具 ── */
		uint64_t GetPixelCount() const { return (uint64_t)m_Width * m_Height; }
		uint64_t GetDataSize()  const { return (uint64_t)m_Width * m_Height * m_Channels; }

		/* 显式拷贝（返回一个新 Image 且拥有独立像素内存） */
		Image Copy() const;

	private:
		/* 内部：从已读取的 Buffer 解码 */
		void DecodeFromBuffer(const Buffer& fileData);

		Buffer      m_Pixels;    // 解码后的像素数据（owned）
		uint32_t    m_Width  = 0;
		uint32_t    m_Height = 0;
		uint32_t    m_Channels = 0;
		bool        m_Loaded = false;
		ImageError  m_Error  = ImageError::None;
	};

} // namespace X_Y
