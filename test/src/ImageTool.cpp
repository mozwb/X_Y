#include "XCore/Decoder/Blueprint/Image.h"

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
	using X_Y::Buffer;
	using X_Y::Decode::Image;

	void WriteU16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
	{
		bytes[offset] = static_cast<uint8_t>(value);
		bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
	}

	void WriteU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
	{
		for (size_t i = 0; i < 4; ++i)
			bytes[offset + i] = static_cast<uint8_t>(value >> (i * 8));
	}

	Buffer MakeBmpFixture()
	{
		std::vector<uint8_t> bytes(62, 0);
		bytes[0] = 'B';
		bytes[1] = 'M';
		WriteU32(bytes, 2, static_cast<uint32_t>(bytes.size()));
		WriteU32(bytes, 10, 54);
		WriteU32(bytes, 14, 40);
		WriteU32(bytes, 18, 2);
		WriteU32(bytes, 22, 1);
		WriteU16(bytes, 26, 1);
		WriteU16(bytes, 28, 24);
		WriteU32(bytes, 34, 8);

		bytes[54] = 0;
		bytes[55] = 0;
		bytes[56] = 255;
		bytes[57] = 0;
		bytes[58] = 255;
		bytes[59] = 0;

		Buffer buffer;
		buffer.Overwrite(bytes.data(), bytes.size());
		return buffer;
	}

	Buffer MakeTgaFixture()
	{
		std::vector<uint8_t> bytes(24, 0);
		bytes[2] = 2;
		WriteU16(bytes, 12, 2);
		WriteU16(bytes, 14, 1);
		bytes[16] = 24;

		bytes[18] = 0;
		bytes[19] = 0;
		bytes[20] = 255;
		bytes[21] = 0;
		bytes[22] = 255;
		bytes[23] = 0;

		Buffer buffer;
		buffer.Overwrite(bytes.data(), bytes.size());
		return buffer;
	}

	bool Check(bool condition, std::string_view description)
	{
		std::cout << (condition ? "[PASS] " : "[FAIL] ") << description << '\n';
		return condition;
	}

	bool IsExpectedRgbImage(const Image &image)
	{
		const auto &pixels = image.GetPixelBuffer();
		return image.IsLoaded() &&
			   image.GetWidth() == 2 &&
			   image.GetHeight() == 1 &&
			   image.GetChannels() == 3 &&
			   pixels.Size == 6 &&
			   pixels[0] == 255 && pixels[1] == 0 && pixels[2] == 0 &&
			   pixels[3] == 0 && pixels[4] == 255 && pixels[5] == 0;
	}

	int RunSelfTest()
	{
		bool passed = true;
		const Buffer bmp = MakeBmpFixture();
		const Buffer tga = MakeTgaFixture();

		Image image = X_Y::Decoder::Decode<Image>(bmp);
		passed &= Check(IsExpectedRgbImage(image), "Decode 2x1 RGB BMP from memory");

		image.Decode(tga);
		passed &= Check(IsExpectedRgbImage(image), "Decode 2x1 RGB TGA into an existing image");

		const Image copy = image.Copy();
		passed &= Check(IsExpectedRgbImage(copy), "Copy decoded pixel data");

		Buffer invalid;
		const uint8_t invalidBytes[] = {0, 1, 2, 3};
		invalid.Overwrite(invalidBytes, sizeof(invalidBytes));
		image.Decode(invalid);
		passed &= Check(!image.IsLoaded() && image.GetWidth() == 0 &&
							image.GetPixelBuffer().Size == 0,
						"Clear previous pixels after invalid input");

		Buffer empty;
		image.Decode(empty);
		passed &= Check(!image.IsLoaded() &&
							image.GetError() == X_Y::Decode::ImageError::InvalidData,
						"Reject empty input");

		std::cout << (passed ? "Image decoder self-test passed\n"
							 : "Image decoder self-test failed\n");
		return passed ? 0 : 1;
	}

	uint64_t HashPixels(const Buffer &pixels)
	{
		uint64_t hash = 14695981039346656037ull;
		for (uint64_t i = 0; i < pixels.Size; ++i)
		{
			hash ^= pixels[i];
			hash *= 1099511628211ull;
		}
		return hash;
	}

	int InspectImage(const std::filesystem::path &path)
	{
		Image image(path);
		if (!image.IsLoaded())
		{
			std::cerr << "[FAIL] " << path.string() << " (error "
					  << static_cast<int>(image.GetError()) << ")\n";
			return 1;
		}

		std::cout << "[IMAGE] " << path.string()
				  << " | " << image.GetWidth() << 'x' << image.GetHeight()
				  << " | " << image.GetChannels() << " channels"
				  << " | " << image.GetDataSize() << " bytes"
				  << " | fnv1a64=0x" << std::hex << std::setw(16)
				  << std::setfill('0') << HashPixels(image.GetPixelBuffer())
				  << std::dec << '\n';
		return 0;
	}
}

int main(int argc, char **argv)
{
	if (argc == 2 && std::string_view(argv[1]) == "--self-test")
		return RunSelfTest();

	if (argc < 2)
	{
		std::cerr << "Usage: XYImageTool --self-test | <image-path> [image-path ...]\n";
		return 2;
	}

	int result = 0;
	for (int i = 1; i < argc; ++i)
		result |= InspectImage(std::filesystem::path(argv[i]));
	return result;
}
