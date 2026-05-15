test_enabled = true

workspace "X_Y"
    architecture "x64"
    configurations { "Debug", "Release", "Dist" }
    systemversion "latest"
    cppdialect "C++20"
    staticruntime "On"

    -- ✅ 全局包含：整个解决方案根目录（所有项目自动生效！）
      includedirs
    {
        ".",            -- 你的引擎根目录
        "vendor"        -- 👈 就加这一行！
    }

    -- ✅ Test 设为启动项目
    if test_enabled then
        startproject "Test"
    end

-- 输出目录（无拼接，纯 Premake 宏，永不报错）
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- 全局编译配置
filter "configurations:Debug"
    symbols "On"
    defines "XY_DEBUG"

filter "configurations:Release"
    optimize "On"
    defines "XY_RELEASE"

filter "configurations:Dist"
    optimize "Full"
    defines "XY_DIST"
    symbols "Off"
  
filter "system:Windows"
    defines "XY_PLATFORM_WINDOWS"  -- Windows 专属宏
filter "system:Linux"
    defines "XY_PLATFORM_LINUX"    -- Linux 专属宏
filter "system:macosx"
    defines "XY_PLATFORM_MAC"      -- macOS 专属宏    

filter {}

-- 引入模块
include "Log"
include "Window"

-- 测试项目
if test_enabled then
project "Test"
    location "Test"
    kind "WindowedApp"
    language "C++"

    -- ✅ 无拼接、无括号、永不报错
    targetdir "%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"
    objdir "%{wks.location}/bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"

    pchheader "xypch.h"
    pchsource "xypch/xypch.cpp"

    files
    {
        "Test/src/**.h",
        "Test/src/**.cpp",
        "xypch/xypch.h",
        "xypch/xypch.cpp"
    }

    -- ✅ 因为全局已经 includedirs "."，这里啥都不用写！
    includedirs { "xypch" }

    links
    {

        "Window"
    }
end