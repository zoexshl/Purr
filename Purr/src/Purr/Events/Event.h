#pragma once

#include "Purr/Core.h"

/*
	Systeme evenement : Application stop des que la souris est clique immediatement
*/
namespace Purr {


	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		AppTick, AppUpdate, AppRender,
		KeyPressed, KeyReleased,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled

	};

	// Dans le cas ou on veux filtrer les evenements
	enum EventCategory
	{
		None = 0,
		EventCategoryApplication = BIT(0), // 00001
		EventCategoryInput		 = BIT(1), // 00010
		EventCategoryKeyboard	 = BIT(2), // 00100
		EventCategoryMouse		 = BIT(3), // 01000
		EventCategoryMouseButton = BIT(4)  // 10000
	};

// Macros
#define EVENT_CLASS_TYPE(type) static EventType GetStaticType(){ return EventType::##type;}\
								virtual EventType GetEventType() const override { return GetStaticType();}\
								virtual const char* GetName() const override { return #type;}


#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlag() const override { return category;}

	class PURR_API Event
	{
		friend class EventDispatcher;

	public:
		
		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlag() const = 0;
		virtual std::string ToString() const { return GetName(); }

		inline bool IsInCategory(EventCategory category)
		{
			return GetCategoryFlag() & category;
		}
	protected:

		bool m_Handled = false;
	};

	class EventDispatcher
	{

		template<typename T>
		using EventFn = std::function<bool(T&)>;
	public:
		EventDispatcher(Event& event)
			: m_Event(event)
		{
		}

		// avant : template<typename T, typename F>
		template<typename T>
		bool Dispatch(EventFn<T> func)
		{
			if (m_Event.GetEventType() == T::GetStaticType())
			{
				m_Event.m_Handled = func(*(T*)&m_Event);
				return true;
			}
			return false;
		}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}

	
	// Specialize fmt::formatter for Event types outside the Hazel namespace
	template<typename T>
	struct fmt::formatter<
		T, std::enable_if_t<std::is_base_of<Purr::Event, T>::value, char>>
		: fmt::formatter<std::string>
	{
		auto format(const T& event, fmt::format_context& ctx) const
		{
			return fmt::format_to(ctx.out(), "{}", event.ToString());
		}
	};

	// Utility function for formatting strings with arguments
	template <typename... T>
	std::string StringFromArgs(fmt::format_string<T...> fmt, T&&... args)
	{
		return fmt::format(fmt, std::forward<T>(args)...);
	}


}
