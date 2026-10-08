#include "XCore/Input/Input.h"

#include <iostream>

int main()
{
	auto device = X_Y::Input::AudioOutputDeviceFactory::CreateDefault();
#ifdef XY_PLATFORM_WINDOWS
	if (!device)
	{
		std::cerr << "No Windows audio output backend is registered\n";
		return 1;
	}
	std::cout << "WASAPI output backend is available\n";
#else
	if (device)
	{
		std::cerr << "Unexpected default audio backend on this platform\n";
		return 1;
	}
	std::cout << "No audio output backend is registered for this platform\n";
#endif

	X_Y::Input::AudioPlayer player;
	if (player.Open("xy_audio_player_missing_fixture.wav") || player.IsOpen() ||
		player.GetErrorMessage().empty())
	{
		std::cerr << "AudioPlayer did not report an invalid input file\n";
		return 1;
	}
	if (player.Play() || player.Stop())
	{
		std::cerr << "AudioPlayer accepted playback controls without an open file\n";
		return 1;
	}
	player.Close();
	if (player.IsOpen())
	{
		std::cerr << "AudioPlayer remained open after Close\n";
		return 1;
	}
	return 0;
}
