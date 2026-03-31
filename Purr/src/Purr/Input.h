#pragma once
#include "Purr/Core.h"


namespace Purr {

	class PURR_API Input {

	public:
		inline static bool IsKeyPressed(int keycode) { return s_Instance->IsKeyPressedImpl(keycode);  }

		inline static bool IsMouseButtonPressed(int button) { return s_Instance->IsMouseButtonPressedImpl(button); }

		inline static float GetMouseX() { return s_Instance->GetMouseXImpl(); }

		inline static float getMouseY() { return s_Instance->GetMouseYImpl(); }

		inline static std::pair<float, float> getMousePosition() { return s_Instance->GetMousePositionImpl(); }

	protected:
		virtual bool IsKeyPressedImpl(int key) = 0;


		virtual bool IsMouseButtonPressedImpl(int button) = 0;

		virtual float  GetMouseXImpl() = 0;

		virtual float GetMouseYImpl() = 0;

		virtual  std::pair<float, float> GetMousePositionImpl() = 0;

	private:
		static Input* s_Instance; // single Instance
	};
}