#include "purrpch.h"
#include "WindowsInput.h"
#include <GLFW/glfw3.h>
#include "Purr/Application.h"

namespace Purr {

	Input* Input::s_Instance = new WindowsInput();

	bool WindowsInput::IsKeyPressedImpl(int keycode)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()) ; // Référence de la fenêtre / Window glfw

		auto state = glfwGetKey(window, keycode);
		
		return state == GLFW_PRESS || state == GLFW_REPEAT; // Retourne -> si key est pressed, ou repeated

	}
	bool WindowsInput::IsMouseButtonPressedImpl(int button)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()); // Référence de la fenêtre / Window glfw

		auto state = glfwGetMouseButton(window, button);

		return state == GLFW_PRESS;
	}

	std::pair<float, float> WindowsInput::GetMousePositionImpl()
	{


		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()); // Référence de la fenêtre / Window glfw
		double xpos, ypos;

		glfwGetCursorPos(window, &xpos, &ypos);

		return { (float)xpos, (float)ypos };

	}

	float WindowsInput::GetMouseXImpl()
	{
		auto [x, y] = GetMousePositionImpl();

		
		return x;
	}

	float WindowsInput::GetMouseYImpl()
	{

		auto [x, y] = GetMousePositionImpl();


		return y;
	}
}