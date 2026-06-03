project "Movement"
    kind "StaticLib"
    language "C++"



    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "include",        -- 只写 include，Core 项目自己的目录
        "%{wks.location}/Core/Log/include",
        "%{wks.location}/Core/Input/include",
        "%{wks.location}/Core/XCore/include",       -- 第三方库要往上一级找
    }
    links{
        "Log",
        "Input"
    }