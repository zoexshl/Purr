#include "purrpch.h"
#include <Purr.h>

class Sandbox : public Purr::Application
{
public:
	Sandbox()
	{

	}

	~Sandbox()
	{

	}
};

Purr::Application* Purr::CreateApplication()
{
	return new Sandbox();
}