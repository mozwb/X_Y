#pragma once
#ifdef HZ_PLATFORM_WINDOWS

extern Hazel::Application* Hazel::createApplication();
int main(int argc, char** argv) {
	auto app = Hazel::createApplication();
	app->Run();
	delete app;
	return 0;
}


#endif