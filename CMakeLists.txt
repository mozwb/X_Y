# ==============================================================================
# X_Y —— 内核根构建
#
# 现在只做一件事：把 Engine/src 编成 X_Y.exe，并把 Memory 整库链进来。
# 构建器（xy）、程序、测试等目录以后按需加。
#
# 注意：Modules 现在还是「源码 + 构建产物」的过渡状态：
#   头：<workspace>/Modules/Modules
#   库：<workspace>/Modules/build/out/lib/libMemory.a
# 等二进制仓库目录定下来，只改这两个 cache 变量，不动下面的规则。
# ==============================================================================
cmake_minimum_required(VERSION 3.24)
project(X_Y LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Debug / Release" FORCE)
endif()

# ── 输出目录 ────────────────────────────────────────────────────────────────
# out/bin：所有能跑 / 能加载的东西（exe + dll/so），xy 和 pkg 永远同目录。
# out/lib：只放编译期库（.a / .lib / import lib）。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/lib")

# ── 平台宏 / 编码 ───────────────────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(XY_PLATFORM_WINDOWS UNICODE _UNICODE)
elseif(APPLE)
    add_compile_definitions(XY_PLATFORM_MAC)
else()
    add_compile_definitions(XY_PLATFORM_LINUX)
endif()

add_compile_definitions(
    $<$<CONFIG:Debug>:XY_DEBUG>
    $<$<CONFIG:Release>:XY_RELEASE>)

if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# ── Modules：过渡期路径 ─────────────────────────────────────────────────────
set(XY_MODULES_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/Modules"
    CACHE PATH "Modules 公开头的 include 根")

if(WIN32)
    set(_xy_memory_lib_default
        "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/build/out/lib/libMemory.dll.a")
    set(_xy_memory_dll_default
        "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/build/out/bin/Memory.dll")
else()
    set(_xy_memory_lib_default
        "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/build-linux/out/lib/libMemory.so")
    set(_xy_memory_dll_default
        "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/build-linux/out/lib/libMemory.so")
endif()

set(XY_MEMORY_LIB "${_xy_memory_lib_default}"
    CACHE FILEPATH "Memory import lib / 共享库")
set(XY_MEMORY_DLL "${_xy_memory_dll_default}"
    CACHE FILEPATH "Memory 运行期 DLL")

if(NOT EXISTS "${XY_MEMORY_LIB}")
    message(FATAL_ERROR
        "找不到 Memory 共享库：${XY_MEMORY_LIB}\n"
        "先在 Modules 里编 Memory（cmake --build Modules/build --target Memory）")
endif()

# ── Memory 依赖包：out/modules/Memory/{include,bin,lib} ─────────────────────
set(XY_OUT_MODULES "${CMAKE_BINARY_DIR}/out/modules")
set(XY_MEMORY_PACKAGE "${XY_OUT_MODULES}/Memory")
file(MAKE_DIRECTORY "${XY_MEMORY_PACKAGE}/include/Memory")
file(COPY "${XY_MODULES_INCLUDE}/Memory/Memory.h"
     DESTINATION "${XY_MEMORY_PACKAGE}/include/Memory")

if(WIN32)
    set(_xy_memory_package_file "bin/Memory.dll")
else()
    set(_xy_memory_package_file "bin/libMemory.so")
endif()

file(WRITE "${XY_MEMORY_PACKAGE}/package.xy"
"name = Memory\n"
"version = 0.0.1\n"
"kind = library\n"
"abi = 1\n"
"include = include\n"
"lib = lib\n"
"file = ${_xy_memory_package_file}\n")

add_library(xy_memory SHARED IMPORTED)
if(WIN32)
    set_target_properties(xy_memory PROPERTIES
        IMPORTED_IMPLIB "${XY_MEMORY_LIB}"
        IMPORTED_LOCATION "${XY_MEMORY_DLL}"
        INTERFACE_INCLUDE_DIRECTORIES "${XY_MEMORY_PACKAGE}/include;${XY_MODULES_INCLUDE}")
else()
    set_target_properties(xy_memory PROPERTIES
        IMPORTED_LOCATION "${XY_MEMORY_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${XY_MEMORY_PACKAGE}/include;${XY_MODULES_INCLUDE}")
endif()

# ── 内核 ────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE XY_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/src/*.cpp")

# 平台目录二选一：Windows 只编 Win32，其它只编 Posix。
if(WIN32)
    list(FILTER XY_ENGINE_SOURCES EXCLUDE REGEX "/Platform/Posix/")
else()
    list(FILTER XY_ENGINE_SOURCES EXCLUDE REGEX "/Platform/Win32/")
endif()

if(NOT XY_ENGINE_SOURCES)
    message(FATAL_ERROR "Engine/src 下没有 .cpp，先放一个 main.cpp")
endif()

add_executable(X_Y ${XY_ENGINE_SOURCES})

target_include_directories(X_Y PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/include")

# 模块 DLL 要反向链 X_Y.exe：生成 import lib，并导出 Memory 等符号。
set_target_properties(X_Y PROPERTIES ENABLE_EXPORTS TRUE)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(X_Y PRIVATE "-Wl,--export-all-symbols")
endif()

if(NOT WIN32)
    target_link_options(X_Y PRIVATE "-rdynamic")
    target_link_options(X_Y PRIVATE "-Wl,-rpath,$ORIGIN")
endif()

# Memory 是独立共享库：X_Y、IO、consumer 共用同一份。
target_link_libraries(X_Y PRIVATE xy_memory)

if(WIN32)
    add_custom_command(TARGET X_Y POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${XY_MEMORY_DLL}" "$<TARGET_FILE_DIR:X_Y>")
endif()

# Memory 依赖包的 bin/lib 随 X_Y 一起落盘。
add_custom_command(TARGET X_Y POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${XY_MEMORY_PACKAGE}/bin" "${XY_MEMORY_PACKAGE}/lib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${XY_MEMORY_DLL}" "${XY_MEMORY_PACKAGE}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${XY_MEMORY_LIB}" "${XY_MEMORY_PACKAGE}/lib")

# ── 程序（微程序 DLL）──────────────────────────────────────────────────────
# Programs/<Name>/CMakeLists.txt 一出现就接进来。
file(GLOB _xy_program_dirs CONFIGURE_DEPENDS LIST_DIRECTORIES true
    "${CMAKE_CURRENT_SOURCE_DIR}/Programs/*")
foreach(_dir IN LISTS _xy_program_dirs)
    if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
        add_subdirectory("${_dir}")
    endif()
endforeach()

# ── 应用（命令 DLL）────────────────────────────────────────────────────────
# Apps/<Name>/CMakeLists.txt 一出现就接进来。
file(GLOB _xy_app_dirs CONFIGURE_DEPENDS LIST_DIRECTORIES true
    "${CMAKE_CURRENT_SOURCE_DIR}/Apps/*")
foreach(_dir IN LISTS _xy_app_dirs)
    if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
        add_subdirectory("${_dir}")
    endif()
endforeach()

# ── 构建器 ─────────────────────────────────────────────────────────────────
add_subdirectory(Builder)
