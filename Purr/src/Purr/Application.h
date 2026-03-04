#pragma once
#include "Core.h"
#include "Events/Event.h"

namespace Purr {
	class PURR_API Application
	{

	public: 
		Application();
		virtual ~Application();

		void Run();
	};
	// Défini dans Client
	Application* CreateApplication();
}


