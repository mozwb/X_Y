project "XMath"
    kind "StaticLib"
    language "C++"


    files
    {
        "include/**.h",
        "src/**.cpp",
        "%{wks.location}/vendor/glm/**.hpp"
    }

    includedirs
    {
        "include",
        "%{wks.location}/vendor/glm"
    }
