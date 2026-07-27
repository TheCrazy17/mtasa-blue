project "GUI"
	language "C++"
	kind "SharedLib"
	targetname "cgui"
	targetdir(buildpath("mta"))
	clangtidy "On"

	filter "system:windows"
		includedirs { "../../vendor/sparsehash/src/windows" }

	filter {}
		includedirs {
			"../../Shared/sdk",
			"../sdk",
			"../../vendor/cegui-0.8.7/include",
			"../../vendor/tinyxml2",
			"../../vendor/freetype/include",
			"../../vendor/sparsehash/src/"
		}

	pchheader "StdInc.h"
	pchsource "StdInc.cpp"

	defines {
		"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING",
		"CEGUI_STATIC"
	}

	links {
		"CEGUI087", "freetype", "tinyxml2",
		"d3dx9.lib",
		"dxerr.lib"
	}

	vpaths {
		["Headers/*"] = "**.h",
		["Sources/*"] = "**.cpp",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"*.h",
		"*.cpp"
	}

	filter "architecture:not x86"
		flags { "ExcludeFromBuild" }

	filter "system:not windows"
		flags { "ExcludeFromBuild" }
