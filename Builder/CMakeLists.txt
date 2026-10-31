# xy —— 最小构建器。
#
# 只链 Platform/Process 的实现；不需要 Memory / IO / X_Y.exe。
# 产物 xy.exe 和 X_Y.exe 放在同一个 out/bin。

add_executable(xy src/main.cpp)

target_include_directories(xy PRIVATE
    "${PROJECT_SOURCE_DIR}/Engine/include"
    "${PROJECT_SOURCE_DIR}/Engine/src")

if(WIN32)
    target_sources(xy PRIVATE
        "${PROJECT_SOURCE_DIR}/Engine/src/Platform/Win32/ProcessWin32.cpp")
else()
    target_sources(xy PRIVATE
        "${PROJECT_SOURCE_DIR}/Engine/src/Platform/Posix/ProcessPosix.cpp")
endif()
