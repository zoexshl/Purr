#pragma once

#ifdef PURR_PLATFORM_WINDOWS
	#ifdef PURR_BUILD_DLL
		#define PURR_API _declspec(dllexport)
	#else
		#define PURR_API _declspec(dllimport)
	#endif
#else
	#error Purr only support Windows !
#endif

#ifdef PURR_ENABLE_ASSERTS
#define PURR_ASSERT(x, ...) { if(!(x)) { PURR_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define PURR_CORE_ASSERT(x, ...) { if(!(x)) { PURR_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
#define PURR_ASSERT(x, ...)
#define PURR_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x) // 1 qui est shifted de x places


