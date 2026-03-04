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

#define BIT(x) (1 << x) // 1 qui est shifted de x places