# ==============================================================================
# X_Y — 标准 CMake 构建（重构版）
# 目录约定：
#   Modules/<Name>/include   对外头文件
#   Modules/<Name>/src       实现
#   vendor/                  第三方(glad/glm/imgui)
#   dist/lib/                编译产物：mingw/ 与 msvc/ 分开放
#   dist/include/            对外头文件汇集(构建后)
# ==============================================================================

cmake_minimum_required(VERSION 3.20)
project(X_Y LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ── 全局编译定义 ─────────────────────────────────
if(WIN32)
    add_compile_definitions(XY_PLATFORM_WINDOWS UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(XY_PLATFORM_LINUX)
elseif(APPLE)
    add_compile_definitions(XY_PLATFORM_MAC)
endif()

# 构建配置宏（Debug/Release/Dist）
add_compile_definitions($<$<CONFIG:Debug>:XY_DEBUG>)
add_compile_definitions($<$<CONFIG:Release>:XY_RELEASE>)
add_compile_definitions($<$<CONFIG:Dist>:XY_DIST>)

# ── UTF-8 编译（源码含中文注释，必须全链路 UTF-8）──
if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# ── 输出目录：dist/lib/<toolset> ─────────────────
if(MSVC)
    set(XY_LIB_DIR "${CMAKE_SOURCE_DIR}/dist/lib/msvc")
else()
    set(XY_LIB_DIR "${CMAKE_SOURCE_DIR}/dist/lib/mingw")
endif()

# ══════════════════════════════════════════════════════
# 辅助函数：定义一个 X_Y 模块静态库
#   xy_module(NAME)   → 用 Modules/NAME 下的 include + src
# ══════════════════════════════════════════════════════
function(xy_module NAME)
    set(_src_dir "${CMAKE_SOURCE_DIR}/Modules/${NAME}")
    file(GLOB_RECURSE _src
        "${_src_dir}/src/*.cpp"
        "${_src_dir}/src/*.h"
        "${_src_dir}/src/*.c")

    add_library(${NAME} STATIC ${_src})

    target_include_directories(${NAME} PUBLIC
        "${_src_dir}"                        # 本模块根：内部 #include "Canvas.h"
        "${CMAKE_SOURCE_DIR}/Modules"       # 跨模块：#include "Widget/Canvas.h"
        "${CMAKE_SOURCE_DIR}")              # vendor/... 从 X_Y 根解析

    set_target_properties(${NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${XY_LIB_DIR}")
endfunction()

# ══════════════════════════════════════════════════════
# vendor：glad(静态库) / glm(header-only，提供 include) / imgui(并入需要它的模块)
# ══════════════════════════════════════════════════════
add_library(glad STATIC "vendor/glad/src/glad.c")
target_include_directories(glad PUBLIC
    "${CMAKE_SOURCE_DIR}/vendor/glad/include")
set_target_properties(glad PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${XY_LIB_DIR}")

# ══════════════════════════════════════════════════════
# 模块（按迁移进度逐步加入）
# ── Core 底层 ──
xy_module(XCore)
target_include_directories(XCore PUBLIC
    "${CMAKE_SOURCE_DIR}/vendor"
    )
xy_module(Middle)


# # XMath：header-only（空壳 Policy + glm），INTERFACE 库，只提供 include 路径
# # 注意 vendor/glm 被拍平(glm.hpp 直接在 vendor/glm/ 下)，include 根是 vendor/
# add_library(XMath INTERFACE)
# target_include_directories(XMath INTERFACE
#     "${CMAKE_SOURCE_DIR}/Modules/XMath"
#     "${CMAKE_SOURCE_DIR}/Modules"
#     "${CMAKE_SOURCE_DIR}/vendor")

# ── Core 上层（有依赖）──
xy_module(DataStore)
target_link_libraries(DataStore PUBLIC XCore)

xy_module(Log)
target_link_libraries(Log PUBLIC DataStore)

xy_module(Image)
target_link_libraries(Image PUBLIC Log XCore)

# ── vendor：freetype(文字后端，per-pixel alpha 透明文字) ──
# add_subdirectory 源码编译，静态链接进 X_Y。关掉非必要外部依赖(zlib/png/svg…)。
set(FT_DISABLE_ZLIB TRUE)
set(FT_DISABLE_BZIP2 TRUE)
set(FT_DISABLE_PNG TRUE)
set(FT_DISABLE_HARFBUZZ TRUE)
set(FT_DISABLE_BROTLI TRUE)
add_subdirectory(vendor/freetype ${CMAKE_BINARY_DIR}/freetype EXCLUDE_FROM_ALL)

# 强制把 FreeType 的库输出到 dist/lib/mingw/
set_target_properties(freetype PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${XY_LIB_DIR}"
)

# ══════════════════════════════════════════════════════
# ── APP 层 ──
# 注：Application 已并入 Widget 模块（Widget/Application.h + src/Application.cpp）
xy_module(Movement)
target_link_libraries(Movement PUBLIC Log XCore)

xy_module(GraphicsContext)
target_link_libraries(GraphicsContext PUBLIC Log glad)

xy_module(Widget)
target_link_libraries(Widget PUBLIC Log  GraphicsContext XCore freetype)

# ── UI（含 imgui，依赖 Widget/DataStore/Timer + opengl32）──
xy_module(UI)
target_link_libraries(UI PUBLIC Widget DataStore XCore)
if(WIN32)
    target_link_libraries(UI PUBLIC opengl32)
endif()
# imgui 源码并入 UI 模块
file(GLOB IMGUI_SRC "vendor/imgui/*.cpp" "vendor/imgui/backends/*.cpp")
target_sources(UI PRIVATE ${IMGUI_SRC})
target_include_directories(UI PUBLIC
    "${CMAKE_SOURCE_DIR}/vendor/imgui"
    "${CMAKE_SOURCE_DIR}/vendor/imgui/backends")

# ══════════════════════════════════════════════════════
# ── Render 层 ──
xy_module(Render)
target_link_libraries(Render PUBLIC Log Middle XCore glad)

xy_module(Model)
target_link_libraries(Model PUBLIC XCore)

# ── Physics：物理 + 碰撞（Pimpl 门面 + 可换后端），依赖自研 XMath ──
xy_module(Physics)
target_link_libraries(Physics PUBLIC XCore)

# ══════════════════════════════════════════════════════
# 分发：把头文件安装到 dist/include（构建后执行，替代旧 MoveFiles.py）
# 用法：cmake --install build --prefix "D:/workbench/X_Y/dist"
#   或  cmake --install build --config Debug --prefix "D:/workbench/X_Y/dist"
# install(DIRECTORY) 自带递归，子目录里的 .h/.hpp 也会一并装进去
# ══════════════════════════════════════════════════════
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Modules/"
        DESTINATION "include"
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp")

