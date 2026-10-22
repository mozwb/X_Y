# ==============================================================================
# XYVendor.cmake —— 第三方依赖的「接线目标」
#
# 模块不直接拼 -I/-l，只链目标名：
#   xy_vendor_glm     Math     （#include "glm/glm.hpp"）
#   xy_vendor_stb     Decoder  （#include "vendor/stb/stb_image.h"）
#   xy_vendor_ffmpeg  Decoder  （libav*，有没有取决于 XY_WITH_FFMPEG）
#
# 具体每个库里有什么是平台相关的事，收在这一个文件；模块 CMakeLists 只管链谁。
# ==============================================================================
include_guard(GLOBAL)

# ── vendor 根目录 ────────────────────────────────────────────────────────────
# 引擎仓库本身应当自带 vendor（glm / stb / FFmpeg 头 + .a）。在它补齐之前，
# 允许指向旁边的工具源码仓库，但**必须显式打印命中了哪一个**，避免悄悄依赖外部路径。
set(XY_VENDOR_DIR "" CACHE PATH "vendor 目录（应含 glm/ 与 stb/）")

if(NOT XY_VENDOR_DIR)
    set(_xy_vendor_candidates
        "${PROJECT_SOURCE_DIR}/vendor"                  # 期望：引擎自带
        "${PROJECT_SOURCE_DIR}/../Modules/vendor"       # 过渡：工具仓库里的第三方
        "${PROJECT_SOURCE_DIR}/../X_YB/vendor")         # 历史路径，仅作兼容
    foreach(_cand IN LISTS _xy_vendor_candidates)
        if(EXISTS "${_cand}/glm" AND EXISTS "${_cand}/stb")
            set(XY_VENDOR_DIR "${_cand}")
            if(NOT _cand STREQUAL "${PROJECT_SOURCE_DIR}/vendor")
                message(STATUS
                    "vendor 取自仓库外部：${_cand}\n"
                    "  这不是长久之计 —— 引擎仓库应当自带 vendor，或由 xy 拉取。")
            endif()
            break()
        endif()
    endforeach()
endif()

if(NOT XY_VENDOR_DIR OR NOT IS_DIRECTORY "${XY_VENDOR_DIR}")
    message(FATAL_ERROR
        "找不到 vendor（需要 glm/ 与 stb/）。已尝试：\n"
        "  ${PROJECT_SOURCE_DIR}/vendor\n"
        "  ${PROJECT_SOURCE_DIR}/../Modules/vendor\n"
        "把第三方库放进 engine 的 vendor/，或用 -DXY_VENDOR_DIR=<path>。")
endif()

# 模块里写的是 #include "vendor/stb/stb_image.h"，所以要 vendor 的父目录
get_filename_component(XY_VENDOR_PARENT "${XY_VENDOR_DIR}" DIRECTORY)

set(XY_FFMPEG_ROOT "${XY_VENDOR_DIR}/FFmpeg" CACHE PATH "FFmpeg 根目录（含 libavcodec/ 等）")
option(XY_WITH_FFMPEG "构建依赖 FFmpeg 的模块（Decoder/Media）" ON)


function(xy_vendor_init)
    message(STATUS "XY_VENDOR_DIR = ${XY_VENDOR_DIR}")

    # ── glm：header-only ──
    if(NOT TARGET xy_vendor_glm)
        add_library(xy_vendor_glm INTERFACE)
        target_include_directories(xy_vendor_glm INTERFACE "${XY_VENDOR_DIR}")
    endif()

    # ── stb：header-only ──
    if(NOT TARGET xy_vendor_stb)
        add_library(xy_vendor_stb INTERFACE)
        target_include_directories(xy_vendor_stb INTERFACE "${XY_VENDOR_PARENT}")
    endif()

    # ── FFmpeg：静态库 / pkg-config，按平台找 ──
    if(TARGET xy_vendor_ffmpeg)
        return()
    endif()

    add_library(xy_vendor_ffmpeg INTERFACE)
    if(NOT XY_WITH_FFMPEG)
        message(STATUS "XY_WITH_FFMPEG=OFF：Decoder/Media 相关部分不参与构建")
        return()
    endif()

    set(_components avformat avcodec swscale swresample avutil)
    set(_ff_libs "")
    foreach(_name IN LISTS _components)
        foreach(_suffix a lib)
            set(_cand "${XY_FFMPEG_ROOT}/lib${_name}/lib${_name}.${_suffix}")
            if(EXISTS "${_cand}")
                list(APPEND _ff_libs "${_cand}")
                break()
            endif()
        endforeach()
    endforeach()

    if(_ff_libs)
        # 本地预编译静态库（MinGW / MSVC 的 .a / .lib）
        target_include_directories(xy_vendor_ffmpeg INTERFACE "${XY_FFMPEG_ROOT}")
        target_link_libraries(xy_vendor_ffmpeg INTERFACE ${_ff_libs})
        if(NOT WIN32)
            target_link_libraries(xy_vendor_ffmpeg INTERFACE m atomic)
        endif()
        message(STATUS "FFmpeg: 本地静态库 ${_ff_libs}")

    else()
        # 系统包（Linux 常见）
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(XY_FFMPEG QUIET IMPORTED_TARGET
                libavformat libavcodec libswscale libswresample libavutil)
        endif()
        if(TARGET PkgConfig::XY_FFMPEG)
            target_link_libraries(xy_vendor_ffmpeg INTERFACE PkgConfig::XY_FFMPEG)
            message(STATUS "FFmpeg: pkg-config ${XY_FFMPEG_VERSION}")
        else()
            message(FATAL_ERROR
                "XY_WITH_FFMPEG=ON 但找不到 FFmpeg。"
                "把预编译库放到 ${XY_FFMPEG_ROOT}/lib<name>/，"
                "或用 -DXY_FFMPEG_ROOT=<path>，或 -DXY_WITH_FFMPEG=OFF。")
        endif()
    endif()
endfunction()
