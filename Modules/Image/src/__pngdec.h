/*
 * __pngdec.h / __pngdec.c 鈥?PNG decoder (pure C, no external dependencies)
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

/* 鈹€鈹€ 瑙ｇ爜缁撴灉鐘舵€?鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
enum PNG_Error
{
    PNG_OK = 0,              /* 鎴愬姛 */
    PNG_ERR_SIGNATURE = 1,   /* 鏂囦欢涓嶆槸 PNG锛堝墠8瀛楄妭涓嶅锛?*/
    PNG_ERR_NO_IHDR = 2,     /* 娌℃壘鍒?IHDR 鍧?*/
    PNG_ERR_NO_IDAT = 3,     /* 娌℃湁鍥惧儚鏁版嵁 */
    PNG_ERR_TRUNCATED = 4,   /* 鏂囦欢琚埅鏂簡锛屾暟鎹笉澶?*/
    PNG_ERR_UNSUPPORT = 5,   /* 涓嶆敮鎸佺殑鏍煎紡锛堜綅娣便€佽壊褰╃被鍨嬨€佷氦缁囩瓑锛?*/
    PNG_ERR_MEMORY = 6,      /* 鍐呭瓨涓嶅 */
    PNG_ERR_BAD_DATA = 7,    /* 鏁版嵁鎹熷潖锛堣В鍘嬪け璐ョ瓑锛?*/
    PNG_ERR_BAD_HUFFMAN = 8, /* 鍝堝か鏇肩爜琛ㄩ敊璇?*/
};

/* 鈹€鈹€ 瑙ｇ爜鍑芥暟 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€ */
/*
 * png_data      - 瀹屾暣 PNG 鏂囦欢鐨勪簩杩涘埗鏁版嵁
 * png_len       - 鏁版嵁闀垮害
 * out_rgba      - [杈撳嚭] 瑙ｇ爜鍚庣殑鍍忕礌鏁版嵁锛岀敤瀹屽悗闇€瑕?free()
 * out_width     - [杈撳嚭] 鍥剧墖瀹藉害
 * out_height    - [杈撳嚭] 鍥剧墖楂樺害
 * out_channels  - [杈撳嚭] 閫氶亾鏁帮紙3=RGB, 4=RGBA锛?
 *
 * 杩斿洖锛歅NG_OK 鎴愬姛锛屽叾浠栧€艰 enum PNG_Error
 */
int png_decode(const unsigned char *png_data, size_t png_len,
               unsigned char **out_rgba,
               unsigned int *out_width, unsigned int *out_height,
               unsigned int *out_channels);

#endif /* __PNGDEC_H */