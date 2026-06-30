project "UI"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"


    files
    {
        "include/**.h",
        "src/**.cpp",
        "%{wks.location}/vendor/imgui/imgui.h",
        "%{wks.location}/vendor/imgui/imgui.cpp",
        "%{wks.location}/vendor/imgui/imgui_draw.cpp",
        "%{wks.location}/vendor/imgui/imgui_tables.cpp",
        "%{wks.location}/vendor/imgui/imgui_widgets.cpp",
        "%{wks.location}/vendor/imgui/imgui_demo.cpp",
        "%{wks.location}/vendor/imgui/backends/imgui_impl_win32.cpp",
        "%{wks.location}/vendor/imgui/backends/imgui_impl_opengl3.cpp"
    }

    includedirs
    {
        "include",
        "%{wks.location}/vendor/imgui",
        "%{wks.location}/vendor/imgui/backends",
        "%{wks.location}/vendor/glad/include"
    }

    filter "system:windows"
        links { "opengl32" }
    filter {}
