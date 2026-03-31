#include "purrpch.h"
#include <Purr.h>
#include <Purr/Events/KeyEvent.h>


class ExampleLayer : public Purr::Layer {
public:
	ExampleLayer() :Layer("Example")
	{

	}

	void OnUpdate() override
	{

		if (Purr::Input::IsKeyPressed(PURR_KEY_TAB))
			PURR_TRACE("-------------- POLL: Tab Key est pressed -------------");

	}

	void OnEvent(Purr::Event& event) override
	{
		// PURR_TRACE("{0}", event);
		if (event.GetEventType() == Purr::EventType::KeyPressed) {
			Purr::KeyPressedEvent& e = (Purr::KeyPressedEvent&)event;
			if(e.GetKeyCode()== PURR_KEY_TAB)
				PURR_TRACE("-------------- EVENT : Tab Key est pressed -------------");
			PURR_TRACE("{0}", (char)e.GetKeyCode());
		}
	}
};



class Sandbox : public Purr::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
		PushOverlay(new Purr::ImGuiLayer());
	}

	~Sandbox()
	{

	}
};

Purr::Application* Purr::CreateApplication()
{
	return new Sandbox();
}