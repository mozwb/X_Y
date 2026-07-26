project "DataStore"
    kind "StaticLib"
    language "C++"

    files {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs {
        "include",
        "%{wks.location}/Core/Buffer/include",
        "%{wks.location}/Core/FilesSystem/include"
    }

    links {
        "Buffer",
        "FilesSystem"
    }
