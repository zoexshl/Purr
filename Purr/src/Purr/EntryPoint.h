#pragma once

#ifdef PURR_PLATFORM_WINDOWS
extern Purr::Application* Purr::CreateApplication();

int main(int argc, char** argv)
{

	auto app = Purr::CreateApplication();
	app->Run();
	delete app; 
}

#endif