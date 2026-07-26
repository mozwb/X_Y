project "Log"
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
        "%{wks.location}/Core/XCore/include",
        "%{wks.location}/Core/Timer/include",
        "%{wks.location}/Core/Buffer/include",
        "%{wks.location}/Core/FilesSystem/include",
        "%{wks.location}/Core/DataStore/include"
    }
    links{
        "DataStore"
    }
