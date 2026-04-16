#pragma once
#include "purrpch.h"
#include "Purr/Core.h"
#include "Purr/Events/Event.h"

namespace Purr {

	/** Mode curseur GLFW, exposé sans dépendre de GLFW dans les applis. */
	enum class CursorMode { Normal, Disabled };

	struct WindowProps
	{
		std::string Title;
		unsigned int Width;
		unsigned int Height;

		WindowProps(const std::string& title = "Purr Engine",
			unsigned int width = 1280,
			unsigned int height = 720)
			: Title(title), Width(width), Height(height)
		{
		}
	};

	// L'interface représentant systeme inf Window
	class PURR_API Window

	{
	public:
		virtual void* GetNativeWindow() const = 0; // Ajouté pour les evenements ImGui
		using EventCallbackFn = std::function<void(Event&)>;
		virtual ~Window(){}
		virtual void OnUpdate() = 0;
		virtual unsigned int GetWidth() const = 0;
		virtual unsigned int GetHeight() const = 0;

		// Les attributs de Windows
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		virtual void SetCursorMode(CursorMode mode) = 0;

		static Window* Create(const WindowProps& props = WindowProps()); // Retourne un pointeur

	};

}