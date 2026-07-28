project "DataStore"
    kind "StaticLib"
    language "C++"

    files {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs {
        "include",
        "%{wks.location}/Core/Memory/include",
        "%{wks.location}/Core/FilesSystem/include"
    }

    links {
        "Memory",
        "FilesSystem"
    }
