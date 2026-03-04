#pragma once

#ifdef PURR_PLATFORM_WINDOWS
extern Purr::Application* Purr::CreateApplication();

int main(int argc, char** argv)
{
	Purr::Log::Init();
	PURR_CORE_WARN("Log initialized !");
	int a = 5;
	PURR_INFO("Hello Var={0}", a);

	auto app = Purr::CreateApplication();
	app->Run();
	delete app; 
}

#endif