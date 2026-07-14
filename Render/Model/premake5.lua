project "Model"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    filter "files:**/ModelGenerator.cpp"
        -- 需要 FilesSystem::WriteFileBinary，无额外依赖

    includedirs
    {
        "include",
        "%{wks.location}/Core/Buffer/include",
        "%{wks.location}/Core/FilesSystem/include",
        "%{wks.location}/Core/Middle/include"
    }

    links
    {
        "Buffer",
        "FilesSystem",
        "Data"
    }
