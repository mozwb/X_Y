# HelloXy —— 最小微程序 DLL。
#
# 只 include xy_api.h，不链 X_Y.exe 的 import lib：
# 它不需要 Memory / Buffer，只用 host 的函数指针。
# 产物名必须是不带 lib 前缀的 HelloXy.dll，load/program 都按这个名字定位。

add_library(HelloXy SHARED src/main.cpp)

target_include_directories(HelloXy PRIVATE
    "${PROJECT_SOURCE_DIR}/Engine/include")

set_target_properties(HelloXy PROPERTIES
    PREFIX ""
    OUTPUT_NAME "HelloXy")

# 只导出 XY_EXPORT 标记的符号（MinGW 会默认自动导出全部）。
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(HelloXy PRIVATE "-Wl,--exclude-all-symbols")
endif()
