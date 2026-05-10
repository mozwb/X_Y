workspace "Log"
    architecture "x64"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
startproject "test"
-- ==========================================
-- Log 库：静态库（给别人调用）
-- ==========================================
project "Log"
    location "Log"        
    kind "StaticLib"      -- 静态库
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }
    includedirs{
        "Log"
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

-- ==========================================
-- test 测试程序：调用 Log 静态库
-- ==========================================
project "test"
    location "test"        
    kind "ConsoleApp"         
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs { "Log/src" }    -- 能找到 Log.h
    links { "Log" }              -- 链接 Log 静态库

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