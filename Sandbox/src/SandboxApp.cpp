#include "purrpch.h"
#include <Purr.h>


class ExampleLayer : public Purr::Layer {
public:
	ExampleLayer() :Layer("Example")
	{

	}

	void OnUpdate() override
	{
		PURR_INFO("ExampleLayer::Update");

	}

	void OnEvent(Purr::Event& event) override
	{
		PURR_TRACE("{0}", event);
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