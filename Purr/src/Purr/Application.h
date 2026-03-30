#pragma once
#include "Core.h"
#include "Events/Event.h"
#include "Window.h"
#include "Purr/Events/ApplicationEvent.h"
#include "Purr/LayerStack.h"

namespace Purr {
	class PURR_API Application
	{

	public: 
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		static inline Application& Get() { return *s_Instance;  }
		inline Window& GetWindow() { return *m_Window; }

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		LayerStack m_LayerStack;

	private: 
		static Application* s_Instance;
	};
	// Défini dans Client
	Application* CreateApplication();
}


