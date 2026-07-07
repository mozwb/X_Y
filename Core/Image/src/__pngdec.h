/*
 * __pngdec.h / __pngdec.c — PNG decoder (pure C, no external dependencies)
 *
 * Based on Corg-Labs/pngdecoder, rewritten for understanding.
 *
 * MIT License
 * Copyright (c) 2026 Corg-Labs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __PNGDEC_H
#define __PNGDEC_H

#include <stddef.h>
#include <stdint.h>

/* ── 解码结果状态 ──────────────────────────────────── */
enum PNG_Error
{
    PNG_OK = 0,              /* 成功 */
    PNG_ERR_SIGNATURE = 1,   /* 文件不是 PNG（前8字节不对） */
    PNG_ERR_NO_IHDR = 2,     /* 没找到 IHDR 块 */
    PNG_ERR_NO_IDAT = 3,     /* 没有图像数据 */
    PNG_ERR_TRUNCATED = 4,   /* 文件被截断了，数据不够 */
    PNG_ERR_UNSUPPORT = 5,   /* 不支持的格式（位深、色彩类型、交织等） */
    PNG_ERR_MEMORY = 6,      /* 内存不够 */
    PNG_ERR_BAD_DATA = 7,    /* 数据损坏（解压失败等） */
    PNG_ERR_BAD_HUFFMAN = 8, /* 哈夫曼码表错误 */
};

/* ── 解码函数 ──────────────────────────────────────── */
/*
 * png_data      - 完整 PNG 文件的二进制数据
 * png_len       - 数据长度
 * out_rgba      - [输出] 解码后的像素数据，用完后需要 free()
 * out_width     - [输出] 图片宽度
 * out_height    - [输出] 图片高度
 * out_channels  - [输出] 通道数（3=RGB, 4=RGBA）
 *
 * 返回：PNG_OK 成功，其他值见 enum PNG_Error
 */
int png_decode(const unsigned char *png_data, size_t png_len,
               unsigned char **out_rgba,
               unsigned int *out_width, unsigned int *out_height,
               unsigned int *out_channels);

#endif /* __PNGDEC_H */