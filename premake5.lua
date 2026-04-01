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
	kind "SharedLib"
	language "C++"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "purrpch.h"
	pchsource "Purr/src/purrpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"opengl32.lib",
		"dwmapi.lib"
	}

	 filter "files:Purr/vendor/**"
        enablepch "Off"
    
    filter ""  -- reset du filtre


	filter "system:windows"
		cppdialect "C++17"
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
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "PURR_RELEASE"
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
		defines "PURR_DIST"
		runtime "Release"
		optimize "On"

	
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	staticruntime "off"

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
		cppdialect "C++17"
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
		optimize "On"

	filter "configurations:Dist"
		defines "PURR_DIST"
		runtime "Release"
		optimize "On"

	