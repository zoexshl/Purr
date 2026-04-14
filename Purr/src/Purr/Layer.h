#pragma once

#include "Purr/Core.h"
#include "Purr/Events/Event.h"

namespace Purr {

	class PURR_API Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		// Virtual : afin d'overwrite les autres layerss au besoin
		virtual void OnAttach(){}
		virtual void OnDetach() {}
		//virtual void OnUpdate(){}
		// delta time (animation)
		virtual void OnUpdate(float dt) {}
		virtual void OnImGuiRender(){}
		virtual void OnEvent(Event& event) {}

		inline const std::string& GetName() const {
			return m_DebugName;
		}
	protected:
		std::string m_DebugName;
	};
}