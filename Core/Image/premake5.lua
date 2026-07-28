project "Image"
    kind "StaticLib"
    language "C++"

    files
    {
        "include/**.h",
        "src/**.h",
        "src/**.c",
        "src/**.cpp"
    }

    includedirs
    {
        "include",
        "src",
        "%{wks.location}/Core/Log/include",
        "%{wks.location}/Core/Memory/include",
        "%{wks.location}/Core/FilesSystem/include"
    }

    links
    {
        "Log",
        "Memory",
        "FilesSystem"
    }
