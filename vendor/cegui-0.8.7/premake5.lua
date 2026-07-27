project "CEGUI087"
	language "C++"
	kind "StaticLib"
	targetname "CEGUI087"
	warnings "Off"

	includedirs {
		"include",
		"../tinyxml2",
		"../freetype/include"
	}

	links { "freetype", "tinyxml2" }

	defines {
		"CEGUI_STATIC",
		"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING"
	}

	vpaths {
		["Headers/*"] = "include/**.h",
		["Sources/*"] = "src/**.cpp",
		["*"] = "premake5.lua"
	}

	files {
		"premake5.lua",
		"src/**.cpp",
		"include/**.h",
	}

	excludes {
		-- Optional features not vendored for this first pass (see include/CEGUI/Config.h)
		"src/MinizipResourceProvider.cpp",
		"src/PCRERegexMatcher.cpp",
	}

	filter "architecture:not x86"
		flags { "ExcludeFromBuild" }

	filter "system:not windows"
		flags { "ExcludeFromBuild" }

	filter {"system:windows"}
		linkoptions { "/ignore:4221" }
		disablewarnings { "4221" }
