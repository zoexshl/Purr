#include "purrpch.h"
#include "Application.h"
#include "Purr/Events/ApplicationEvent.h"
#include "Purr/Log.h"

namespace Purr {


	Application::Application() 
	{

	}

	Application::~Application() 
	{

	}

	void Application::Run()
	{
		WindowResizeEvent e(1200, 720);
		
		if (e.IsInCategory(EventCategoryApplication))
		{
			PURR_TRACE(e.ToString());

		}
		if (e.IsInCategory(EventCategoryInput))
		{
			PURR_TRACE(e.ToString());
		}

		while (true);
	}
}