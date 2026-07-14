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
    "%{wks.location}",
    "%{wks.location}/Core/XCore/include",
    "%{wks.location}/Core/Log/include",
    "%{wks.location}/Core/XMath/include",
    "%{wks.location}/Core/Timer/include",
    "%{wks.location}/Core/Middle/include",
    "%{wks.location}/Core/Input/include",
    "%{wks.location}/APP/Movement/include",
    "%{wks.location}/APP/GraphicsContext/include",
    "%{wks.location}/vendor/glad/include",
    "%{wks.location}/vendor/glm"
    }

    links
    {
        "Log",
        "Middle",
        "XCore"
    }
