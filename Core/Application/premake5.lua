project "Application"
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
        "%{wks.location}/Core/XCore/include",
    }
    links {
        "Log"
    }