# pkg —— 只读包管理命令 DLL。
#
# 只 include Engine/include 的 ABI 头，不链 X_Y.exe：
# 它只用 std::filesystem / fstream，不需要 Memory / IO。
# 产物名必须是不带 lib 前缀的 pkg.dll，load 时按这个名字定位。

add_library(pkg SHARED src/main.cpp)

target_include_directories(pkg PRIVATE
    "${PROJECT_SOURCE_DIR}/Engine/include")

set_target_properties(pkg PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pkg")

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(pkg PRIVATE "-Wl,--exclude-all-symbols")
endif()
