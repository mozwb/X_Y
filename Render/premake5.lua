project "Render"
    location "."
    kind "StaticLib"
    language "C++"


    files {
        "src/**.h",
        "src/**.cpp",
        "vendor/glad/src/**.c"
    }

    includedirs { "src",
    "vendor/glad/include" 
    }