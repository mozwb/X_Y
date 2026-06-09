project "Render"
    location "."
    kind "StaticLib"
    language "C++"


    files {
        "include/**.h",
        "src/**.cpp",
         "%{wks.location}/vendor/glad/src/glad.c"
    }

    includedirs { 
    "include",
    "%{wks.location}/vendor/glad/include"       
    }