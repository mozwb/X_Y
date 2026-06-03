project "Log"
    kind "StaticLib"
    language "C++"



    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "include",  
        "%{wks.location}/Core/XCore/include"      -- 只写 include，Core 项目自己的目录
    }
    links{
    }