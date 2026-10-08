#include "XCore/Decoder/Blueprint/Media.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using X_Y::Decode::Media;
	using X_Y::Decode::MediaFrame;
	using X_Y::Decode::MediaReadStatus;
	using X_Y::Decode::MediaStreamType;
	using X_Y::Decoder;

	void WriteU16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
	{
		bytes[offset] = static_cast<uint8_t>(value);
		bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
	}

	void WriteU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
	{
		for (size_t index = 0; index < 4; ++index)
			bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
	}

	std::filesystem::path MakeWaveFixture()
	{
		const std::filesystem::path path =
			std::filesystem::temp_directory_path() / "xy_ffmpeg_decoder_self_test.wav";
		std::vector<uint8_t> bytes(50, 0);
		bytes[0] = 'R';
		bytes[1] = 'I';
		bytes[2] = 'F';
		bytes[3] = 'F';
		WriteU32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
		bytes[8] = 'W';
		bytes[9] = 'A';
		bytes[10] = 'V';
		bytes[11] = 'E';
		bytes[12] = 'f';
		bytes[13] = 'm';
		bytes[14] = 't';
		bytes[15] = ' ';
		WriteU32(bytes, 16, 16);
		WriteU16(bytes, 20, 1);
		WriteU16(bytes, 22, 1);
		WriteU32(bytes, 24, 8000);
		WriteU32(bytes, 28, 16000);
		WriteU16(bytes, 32, 2);
		WriteU16(bytes, 34, 16);
		bytes[36] = 'd';
		bytes[37] = 'a';
		bytes[38] = 't';
		bytes[39] = 'a';
		WriteU32(bytes, 40, 6);
		WriteU16(bytes, 44, 16384);
		WriteU16(bytes, 46, static_cast<uint16_t>(-16384));
		WriteU16(bytes, 48, 32767);

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file)
			return {};
		file.write(reinterpret_cast<const char *>(bytes.data()),
				   static_cast<std::streamsize>(bytes.size()));
		return file ? path : std::filesystem::path{};
	}

	std::filesystem::path MakeY4mFixture()
	{
		const std::filesystem::path path =
			std::filesystem::temp_directory_path() / "xy_ffmpeg_decoder_self_test.y4m";
		const std::string header = "YUV4MPEG2 W2 H2 F25:1 Ip A1:1 C444\n";
		const std::string frameMarker = "FRAME\n";
		std::vector<uint8_t> bytes(header.begin(), header.end());
		for (int frame = 0; frame < 2; ++frame)
		{
			bytes.insert(bytes.end(), frameMarker.begin(), frameMarker.end());
			bytes.insert(bytes.end(), 4, frame == 0 ? 76 : 16);
			bytes.insert(bytes.end(), 4, frame == 0 ? 85 : 128);
			bytes.insert(bytes.end(), 4, frame == 0 ? 255 : 128);
		}

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file)
			return {};
		file.write(reinterpret_cast<const char *>(bytes.data()),
				   static_cast<std::streamsize>(bytes.size()));
		return file ? path : std::filesystem::path{};
	}

	bool Check(bool condition, std::string_view description)
	{
		std::cout << (condition ? "[PASS] " : "[FAIL] ") << description << '\n';
		return condition;
	}

	int RunSelfTest()
	{
		const std::filesystem::path path = MakeWaveFixture();
		if (path.empty())
		{
			std::cerr << "[FAIL] Could not create the WAV fixture\n";
			return 1;
		}

		bool passed = true;
		Media decoder = Decoder::Decode<Media>(path);
		const bool opened = decoder.IsOpen();
		passed &= Check(
			opened && decoder.GetInfo().HasAudio && !decoder.GetInfo().HasVideo &&
				decoder.GetInfo().Audio.SampleRate == 8000 &&
				decoder.GetInfo().Audio.Channels == 1,
			"Open a WAV file and inspect its audio stream");
		if (opened)
		{
			MediaFrame frame;
			const MediaReadStatus status = Decoder::DecodeNext(decoder, frame);
			bool samplesMatch = status == MediaReadStatus::Frame &&
				frame.Type == MediaStreamType::Audio &&
				frame.SampleRate == 8000 &&
				frame.Channels == 1 &&
				frame.SampleCount == 3 &&
				frame.Data.Size == 3 * sizeof(float);
			if (samplesMatch)
			{
				const auto *samples = reinterpret_cast<const float *>(frame.Data.Data);
				samplesMatch =
					std::abs(samples[0] - 0.5f) < 0.0001f &&
					std::abs(samples[1] + 0.5f) < 0.0001f &&
					std::abs(samples[2] - 32767.0f / 32768.0f) < 0.0001f;
			}
			passed &= Check(samplesMatch, "Decode PCM audio to interleaved float samples");
			passed &= Check(
				Decoder::DecodeNext(decoder, frame) == MediaReadStatus::EndOfStream,
				"Report end of stream after the final audio frame");
		}

		std::ifstream waveFile(path, std::ios::binary);
		const std::vector<uint8_t> waveBytes{
			std::istreambuf_iterator<char>{waveFile},
			std::istreambuf_iterator<char>{}};
		waveFile.close();
		X_Y::Buffer waveBuffer;
		waveBuffer.Overwrite(waveBytes.data(), waveBytes.size());
		X_Y::BufferStream bufferStream(waveBuffer.View(0, 0));
		Media streamedDecoder = Decoder::Decode<Media>(bufferStream);
		MediaFrame bufferedFrame;
		passed &= Check(
			streamedDecoder.IsOpen() &&
				streamedDecoder.GetInfo().HasAudio &&
				Decoder::DecodeNext(streamedDecoder, bufferedFrame) == MediaReadStatus::Frame &&
				bufferedFrame.SampleCount == 3,
			"Decode media from the shared in-memory byte stream");

		decoder.Close();
		const std::filesystem::path missingPath = path.string() + ".missing";
		Media missingDecoder = Decoder::Decode<Media>(missingPath);
		passed &= Check(
			!missingDecoder.IsOpen() &&
				missingDecoder.GetError() == X_Y::Decode::MediaError::FileNotFound,
			"Report a missing media file");

		std::error_code error;
		std::filesystem::remove(path, error);
		if (error)
		{
			std::cerr << "[FAIL] Could not remove the WAV fixture: " << error.message() << '\n';
			passed = false;
		}

		const std::filesystem::path videoPath = MakeY4mFixture();
		if (videoPath.empty())
		{
			std::cerr << "[FAIL] Could not create the Y4M fixture\n";
			return 1;
		}

		Media videoDecoder = Decoder::Decode<Media>(videoPath);
		const bool videoOpened = videoDecoder.IsOpen();
		passed &= Check(
			videoOpened && videoDecoder.GetInfo().HasVideo &&
				!videoDecoder.GetInfo().HasAudio &&
				videoDecoder.GetInfo().Video.Width == 2 &&
				videoDecoder.GetInfo().Video.Height == 2 &&
				std::abs(videoDecoder.GetInfo().Video.FramesPerSecond - 25.0) < 0.001,
			"Open a Y4M file and inspect its video stream");
		if (videoOpened)
		{
			MediaFrame firstFrame;
			MediaFrame secondFrame;
			const MediaReadStatus firstStatus = Decoder::DecodeNext(videoDecoder, firstFrame);
			const MediaReadStatus secondStatus = Decoder::DecodeNext(videoDecoder, secondFrame);
			passed &= Check(
				firstStatus == MediaReadStatus::Frame &&
					firstFrame.Type == MediaStreamType::Video &&
					firstFrame.Width == 2 &&
					firstFrame.Height == 2 &&
					firstFrame.Data.Size == 2 * 2 * 4 &&
					firstFrame.Data[3] == 255,
				"Decode Y4M video to tightly packed RGBA8");
			passed &= Check(
				secondStatus == MediaReadStatus::Frame &&
					secondFrame.TimestampSeconds > firstFrame.TimestampSeconds &&
					Decoder::DecodeNext(videoDecoder, secondFrame) == MediaReadStatus::EndOfStream,
				"Decode timestamped video frames and report end of stream");
		}

		videoDecoder.Close();
		std::filesystem::remove(videoPath, error);
		if (error)
		{
			std::cerr << "[FAIL] Could not remove the Y4M fixture: " << error.message() << '\n';
			passed = false;
		}

		std::cout << (passed ? "Media decoder self-test passed\n"
							 : "Media decoder self-test failed\n");
		return passed ? 0 : 1;
	}
}

int main(int argc, char **argv)
{
	if (argc == 2 && std::string_view(argv[1]) == "--self-test")
		return RunSelfTest();

	std::cerr << "Usage: XYMediaTool --self-test\n";
	return 2;
}
