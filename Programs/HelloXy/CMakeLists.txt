# HelloXy —— 可运行程序（DLL）。引擎用 LoadLibrary 载入，取 xy_program_v1 调用。
# 名字就是身份：产物必须叫 HelloXy.dll（不带 lib 前缀），由 XYModule.cmake 的 PREFIX "" 保证。

xy_module(HelloXy SHARED DIR "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(HelloXy PRIVATE "${PROJECT_SOURCE_DIR}/Engine/include")
