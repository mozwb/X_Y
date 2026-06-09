project "Math"
    kind "StaticLib"
    language "C++"


    files
    {
        "include/**.h",
        "src/**.cpp",
        "vendor/glm/**.hpp"
    }

    includedirs
    {
        "include",
        "vendor/glm"
    }