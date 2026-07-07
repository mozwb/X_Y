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
        "src"
    }
