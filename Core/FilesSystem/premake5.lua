project "FilesSystem"
    kind "StaticLib"
    language "C++"



    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "include",
        "%{wks.location}/Core/Buffer/include"
    }