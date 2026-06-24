project "Buffer"
    kind "StaticLib"
    language "C++"


    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "include"
    }