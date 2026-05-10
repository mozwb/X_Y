-- 模块开关：默认不启用 test
local enable_test = false

-- 如果外部没工作区，自己建
if not workspace() then
    workspace "Log"
        architecture "x64"
        configurations
        {
            "Debug",
            "Release",
            "Dist"
        }
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
end

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- ===================== Log 静态库 =====================
project "Log"
    location "Log"        
    kind "StaticLib"
    language "C++"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "Log/src/**.h",
        "Log/src/**.cpp"
    }

    includedirs{
        "Log/src"
    }
    externalincludedirs{
        "Log/src"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

    filter "configurations:Debug"
        defines "XY_DEBUG"
        symbols "On"

    filter "configurations:Release or Dist"
        defines "XY_RELEASE"
        optimize "On"

    filter "configurations:Dist"
        defines "XY_DIST"
        optimize "Full"

-- ===================== test 仅开关开启时才生成 =====================
if enable_test then
project "test"
    location "test"        
    kind "ConsoleApp"         
    language "C++"
    startproject "test"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "test/src/**.h",
        "test/src/**.cpp"
    }

    includedirs { "Log/src" }
    links { "Log" }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

    filter "configurations:Debug"
        defines "XY_DEBUG"
        symbols "On"

    filter "configurations:Release or Dist"
        defines "XY_RELEASE"
        optimize "On"

    filter "configurations:Dist"
        defines "XY_DIST"
        optimize "Full"
end