project "Widget"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"


    files {
        "include/**.h",
        "src/**.cpp",
        "enginesrc/**.h",
        "enginesrc/**.cpp"
    }
    includedirs {
        "include",
        "enginesrc",
        "%{wks.location}/vendor/glm",
        "%{wks.location}/Core/Log/include",
        "%{wks.location}/Core/Input/include",
        "%{wks.location}/APP/Movement/include",
        "%{wks.location}/Core/XCore/include",
        "%{wks.location}/APP/GraphicsContext/include",
        "%{wks.location}/APP/Application/include"
    }

    links {
        "Log",
        "Input",
        "GraphicsContext",
        "Application"
    }
