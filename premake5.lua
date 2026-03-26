workspace "Purr"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Ajouter les r�pertoires relatifs au dossier root (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Purr/vendor/GLFW/include"
IncludeDir["Glad"] = "Purr/vendor/Glad/include"
-- Inclure le premake5.lua situ� dans le dossier GLFW
include "Purr/vendor/GLFW" 
include "Purr/vendor/Glad" 

project "Purr"
	location "Purr"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "purrpch.h"
	pchsource "Purr/src/purrpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}"
	}

	links
	{
		"GLFW",
		"Glad",
		"opengl32.lib",
		"dwmapi.lib"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"PURR_PLATFORM_WINDOWS",
			"PURR_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

		postbuildcommands
	{
		("{MKDIR} ../bin/" .. outputdir .. "/Sandbox"),
		("{COPYFILE} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
	}
	
	filter "configurations:Debug"
		defines "PURR_DEBUG"
		buildoptions "/MDd"
		symbols "On"

	filter "configurations:Release"
		defines "PURR_RELEASE"
		buildoptions "/MD"
		optimize "On"

	filter "configurations:Dist"
		defines "PURR_DIST"
		buildoptions "/MD"
		optimize "On"

	
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")


	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}

	includedirs
	{
		"Purr/vendor/spdlog/include",
		"Purr/src"
	}

	links
	{
		"Purr"
	}
	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"PURR_PLATFORM_WINDOWS"
		}
	
	filter "configurations:Debug"
		defines "PURR_DEBUG"
		buildoptions "/MDd"
		symbols "On"

	filter "configurations:Release"
		defines "PURR_RELEASE"
		buildoptions "/MD"
		optimize "On"

	filter "configurations:Dist"
		defines "PURR_DIST"
		buildoptions "/MD"
		optimize "On"

	