project "Window"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"


    files {
        "include/**.h",
        "src/**.cpp"
    }
    includedirs {
        "include",
        "%{wks.location}/Core/Log/include",
        "%{wks.location}/Core/Input/include",
        "%{wks.location}/Core/Movement/include",
        "%{wks.location}/Core/XCore/include",
        "%{wks.location}/Core/GraphicsContext/include",
        "%{wks.location}/Core/Application/include"
    }

    links {
        "Log",
        "Input",
        "GraphicsContext",
        "Application"
    }
