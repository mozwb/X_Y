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
        "vendor",
        "Core"
    }

    -- ✅ Test 设为启动项目
    if test_enabled then
        startproject "Test"
    end

    -- 全局统一输出，全工程共用
    local outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

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

include "Render"
include "vendor/glad" 
group "Core"
include "Core/Log"
include "Core/Input"
include "Core/Movement"
include "Core/GraphicsContext"
include "Core/XCore"
include "Core/Application"
include "Core/Window"
include "Core/Math"
include "Core/Timer"
include "Core/Buffer"
include "Core/FilesSystem"
include "Core/UI"
include "Core/Image"
include "Core/Model"
group ""

-- 测试项目
if test_enabled then
project "Test"
    location "Test"
    kind "WindowedApp"
    -- entrypoint "WinMain"
    language "C++"

  
    pchheader "xypch.h"
    pchsource "xypch/xypch.cpp"

    files
    {
        "Test/src/**.h",
        "Test/src/**.cpp",
        "xypch/xypch.h",
        "xypch/xypch.cpp",
    }

    -- ✅ 因为全局已经 includedirs "."，这里啥都不用写！
    includedirs { 
    "xypch",
    "vendor/glad/include" }
    links
    {
        "Window",
        "Application",
        "GraphicsContext",
        "Render",
        "opengl32",
        "glad",
        "Timer",
        "Buffer",
        "UI",
        "FilesSystem",
        "Image",
        "Model"
    }
end