project "Image"
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