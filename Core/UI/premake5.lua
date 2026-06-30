project "UI"
    kind "StaticLib"
    language "C++"


    files
    {
        "include/**.h",
        "src/**.cpp",
    }

    includedirs
    {
        "include",
        "%{wks.location}/vendor/imgui",
        "%{wks.location}/vendor/imgui/backends"
    }
    links
    {
        "ImGui"
    }
    dependson { "ImGui" }

    filter "configurations:*"
        libdirs { "%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/ImGui" }
    filter {}