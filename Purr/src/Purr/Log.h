#pragma once
#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace Purr {

	class PURR_API Log
	{

	public:
		static void Init();

		inline static std::shared_ptr <spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr <spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr <spdlog::logger> s_CoreLogger; 
		static std::shared_ptr <spdlog::logger> s_ClientLogger;
	};

}

// Macro Core Log
#define PURR_CORE_TRACE(...)	::Purr::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define PURR_CORE_INFO(...)		::Purr::Log::GetCoreLogger()->info(__VA_ARGS__)
#define PURR_CORE_WARN(...)		::Purr::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define PURR_CORE_ERROR(...)	::Purr::Log::GetCoreLogger()->error(__VA_ARGS__)
#define PURR_CORE_FATAL(...)	::Purr::Log::GetCoreLogger()->fatal(__VA_ARGS__)

// Macro Client Log
#define PURR_TRACE(...)			::Purr::Log::GetClientLogger()->trace(__VA_ARGS__)
#define PURR_INFO(...)			::Purr::Log::GetClientLogger()->info(__VA_ARGS__)
#define PURR_WARN(...)			::Purr::Log::GetClientLogger()->warn(__VA_ARGS__)
#define PURR_ERROR(...)			::Purr::Log::GetClientLogger()->error(__VA_ARGS__)
#define PURR_FATAL(...)			::Purr::Log::GetClientLogger()->fatal(__VA_ARGS__)