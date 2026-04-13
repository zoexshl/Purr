workspace "Purr"
	architecture "x64"
	
	startproject "Sandbox"

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
IncludeDir["ImGui"] = "Purr/vendor/imgui"
IncludeDir["glm"] = "Purr/vendor/glm"
-- Inclure le premake5.lua situ� dans le dossier GLFW
include "Purr/vendor/GLFW" 
include "Purr/vendor/Glad" 
include "Purr/vendor/imgui"
group "Dependencies"
	include "Purr/vendor/GLFW"
	include "Purr/vendor/Glad"
	include "Purr/vendor/imgui"

group ""



project "Purr"
	location "Purr"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "purrpch.h"
	pchsource "Purr/src/purrpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",
		"%{prj.name}/vendor/stb_image/stb_image.h",     
		"%{prj.name}/vendor/stb_image/stb_image.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}


	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{prj.name}/vendor/stb_image"
	}


buildoptions { "/external:W0" }

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"opengl32.lib",
		"dwmapi.lib"
	}


	-- Link warning suppression
	-- LNK4006: Sympbol already defined in another library will pick first definition
	linkoptions { "/ignore:4006" }

	 filter "files:Purr/vendor/**"
        enablepch "Off"
    
    filter ""  -- reset du filtre


	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"PURR_PLATFORM_WINDOWS",
			"PURR_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}


	filter "configurations:Debug"
		defines "PURR_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "PURR_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "PURR_DIST"
		runtime "Release"
		optimize "on"

	
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

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
		"Purr/src",
		"Purr/vendor",
		"%{IncludeDir.glm}"
	
	}

	links
	{
		"Purr"
	}
	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
			"PURR_PLATFORM_WINDOWS"
		}
	
	filter "configurations:Debug"
		defines "PURR_DEBUG"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "PURR_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "PURR_DIST"
		runtime "Release"
		optimize "on"
