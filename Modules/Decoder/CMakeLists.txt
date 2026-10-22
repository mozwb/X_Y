# Decoder —— 文本编码 / 图片(stb) / 媒体(FFmpeg)。
#
# 平台：Decoder/Decoder.h（公开头）现在直接 include <windows.h>，非 Windows 编不了，
#       本平台先跳过；等把这个 include 从公开头挪走后就能跨过去。
# 第三方：stb 只 get 头（#include "vendor/stb/stb_image.h"），FFmpeg 由 xy_vendor_ffmpeg 提供。

if(NOT WIN32)
    message(STATUS "Decoder: Decoder.h 直接 include <windows.h>，本平台跳过（待修）")
    return()
endif()

if(NOT TARGET Files)
    message(STATUS "Decoder: 依赖 Files，而 Files 在本平台被跳过 → 一并跳过")
    return()
endif()

# XY_WITH_FFMPEG=OFF 时把 Media 实现摘掉，其余（文本/图片）照常
set(_exclude "")
if(NOT XY_WITH_FFMPEG)
    list(APPEND _exclude "src/Media\\.cpp$")
    message(STATUS "Decoder: XY_WITH_FFMPEG=OFF，排除 src/Media.cpp")
endif()

xy_module(Decoder DEPS Core Memory Files EXCLUDE ${_exclude})

target_link_libraries(Decoder PRIVATE xy_vendor_stb)

if(XY_WITH_FFMPEG AND TARGET xy_vendor_ffmpeg)
    target_link_libraries(Decoder PUBLIC xy_vendor_ffmpeg)
endif()

# 本模块代码自己用到的 Windows 系统库（avutil 的随机数种子走 advapi32/bcrypt）
target_link_libraries(Decoder PUBLIC bcrypt advapi32 ole32 uuid user32)
