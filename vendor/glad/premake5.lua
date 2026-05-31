project "glad"
    kind "StaticLib"
    language "C"
    staticruntime "On"
    systemversion "latest"
    
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

      -- 🔴 关键：彻底禁用预编译头
    pchheader ""
    pchsource ""

    -- 🔴 强制给 VS 加上 /Y- 选项，禁用预编译头
    filter "configurations:*"
        buildoptions { "/Y-" }
    filter {}


    files
    {
        "src/glad.c"
    }
    includedirs
    {
        "include"
    }

    -- Windows 下必须链接 opengl32.lib
    filter "system:windows"
        links { "opengl32" }
    filter {}