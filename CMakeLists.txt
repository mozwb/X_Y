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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/lib")
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
set(XY_MEMORY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/build/out/lib/libMemory.a"
    CACHE FILEPATH "Memory 静态库")

if(NOT EXISTS "${XY_MEMORY_LIB}")
    message(FATAL_ERROR
        "找不到 Memory 静态库：${XY_MEMORY_LIB}\n"
        "先在 Modules 里编 Memory（cmake --build Modules/build --target Memory）")
endif()

add_library(xy_memory STATIC IMPORTED)
set_target_properties(xy_memory PROPERTIES
    IMPORTED_LOCATION "${XY_MEMORY_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${XY_MODULES_INCLUDE}")

if(WIN32)
    target_link_libraries(xy_memory INTERFACE psapi)
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
endif()

# Memory 必须整库进 exe：DLL 用到的 Buffer/统计符号 exe 也得有。
target_link_libraries(X_Y PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,xy_memory>")
