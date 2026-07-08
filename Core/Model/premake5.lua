project "Model"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "include"
    }

    links
    {
        "Buffer",
        "FilesSystem"
    }
