project "GraphicsContext"
    kind "StaticLib"
    language "C++"
    files
    {
        "include/**.h",
        "src/**.cpp",
        "%{wks.location}/vendor/glad/src/glad.c"
    }

    includedirs
    {
        "include",        -- 只写 include，Core 项目自己的目录
        "%{wks.location}/Core/Log/include",       -- 第三方库要往上一级找
        "%{wks.location}/vendor/glad/include"       -- 第三方库要往上一级找
    }
    links{
        "Log"
    }