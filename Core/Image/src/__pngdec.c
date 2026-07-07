/*
 * __pngdec.c — PNG decoder (pure C, no external dependencies)
 *
 * Implements:
 *   - PNG signature & chunk parsing
 *   - IHDR parsing (width, height, bit-depth, color type)
 *   - INFLATE / DEFLATE decompression (RFC 1951)
 *   - PNG adaptive filter reconstruction (types 0-4)
 *   - Output to raw RGBA bytes
 *
 * Currently supports:
 *   - 8-bit RGB (color type 2) → 3 channels
 *   - 8-bit RGBA (color type 6) → 4 channels
 *   - Non-interlaced only
 */

#include "__pngdec.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* ── Error handling via setjmp/longjmp ────────────────── */
static jmp_buf g_err_jmp;
static int g_err_code;
#define DIE(code)              \
    do                         \
    {                          \
        g_err_code = (code);   \
        longjmp(g_err_jmp, 1); \
    } while (0)

/* ════════════════════════════════════════════════════════════
   BIT READER — consumes bits LSB-first (DEFLATE order)
   ════════════════════════════════════════════════════════════ */
typedef struct
{
    const uint8_t *buf;
    size_t len;
    size_t pos;    /* byte position */
    uint32_t bits; /* bit accumulator */
    int nbits;     /* valid bits in accumulator */
} BitReader;
// 初始化：绑定数据，重置状态

static void br_init(BitReader *br, const uint8_t *buf, size_t len)
{
    br->buf = buf;
    br->len = len;
    br->pos = 0;
    br->bits = 0;
    br->nbits = 0;
}

// 填充位累加器，尽量保证 nbits >= 24
static void br_fill(BitReader *br)
{
    while (br->nbits <= 24 && br->pos < br->len)
    {
        br->bits |= (uint32_t)br->buf[br->pos++] << br->nbits;
        br->nbits += 8;
    }
}
// 读取 n 个 bit（LSB 优先），返回结果
static uint32_t br_read(BitReader *br, int n)
{
    if (n == 0)
        return 0;
    br_fill(br);
    if (br->nbits < n)
        DIE(PNG_ERR_TRUNCATED);
    uint32_t v = br->bits & ((1u << n) - 1);
    br->bits >>= n;
    br->nbits -= n;
    return v;
}
// 将位读取器对齐到字节边界
static void br_byte_align(BitReader *br)
{
    int rem = br->nbits & 7;
    if (rem)
    {
        br->bits >>= rem;
        br->nbits -= rem;
    }
}

/* ════════════════════════════════════════════════════════════
   HUFFMAN TABLE (canonical)
   ════════════════════════════════════════════════════════════ */
#define MAX_HUFF_SYMS 288
#define MAX_CODE_LEN 16

typedef struct
{
    uint16_t sym[MAX_HUFF_SYMS];
    uint16_t count[MAX_CODE_LEN + 1];
    uint16_t first_code[MAX_CODE_LEN + 1];
    uint16_t first_sym[MAX_CODE_LEN + 1];
    int max_len;
} HuffTable;
// 构建哈夫曼表（canonical）
static void huff_build(HuffTable *h, const uint8_t *lengths, int nsyms)
{
    memset(h->count, 0, sizeof h->count);
    h->max_len = 0;
    for (int i = 0; i < nsyms; i++)
    {
        h->count[lengths[i]]++;
        if (lengths[i] > h->max_len)
            h->max_len = lengths[i];
    }
    h->count[0] = 0;

    uint16_t pos[MAX_CODE_LEN + 1];
    pos[0] = 0;
    for (int i = 1; i <= h->max_len; i++)
        pos[i] = pos[i - 1] + h->count[i - 1];

    for (int i = 0; i < nsyms; i++)
        if (lengths[i])
            h->sym[pos[lengths[i]]++] = (uint16_t)i;

    uint16_t idx = 0;
    for (int i = 1; i <= h->max_len; i++)
    {
        h->first_sym[i] = idx;
        idx += h->count[i];
    }

    uint16_t code = 0;
    for (int i = 1; i <= h->max_len; i++)
    {
        h->first_code[i] = code;
        code = (uint16_t)((code + h->count[i]) << 1);
    }
}
// 解码哈夫曼码，返回符号
static int huff_decode(BitReader *br, const HuffTable *h)
{
    uint32_t code = 0;
    for (int len = 1; len <= h->max_len; len++)
    {
        code = (code << 1) | br_read(br, 1);
        if (code < (uint32_t)(h->first_code[len] + h->count[len]))
        {
            return h->sym[h->first_sym[len] + (int)(code - h->first_code[len])];
        }
    }
    DIE(PNG_ERR_BAD_HUFFMAN);
    return -1;
}

//DEFLATE decompressor

// Fixed literal/length codes (RFC 1951 §3.2.6) 
static void make_fixed_tables(HuffTable *lit, HuffTable *dist)
{
    uint8_t ll[288], dl[32];
    int i;
    for (i = 0; i <= 143; i++)
        ll[i] = 8;
    for (i = 144; i <= 255; i++)
        ll[i] = 9;
    for (i = 256; i <= 279; i++)
        ll[i] = 7;
    for (i = 280; i <= 287; i++)
        ll[i] = 8;
    for (i = 0; i < 32; i++)
        dl[i] = 5;
    huff_build(lit, ll, 288);
    huff_build(dist, dl, 32);
}

static const int length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const int length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const int dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const int dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* Dynamic block: code length alphabet order */
static const int cl_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* Dynamic output buffer */
typedef struct
{
    uint8_t *data;
    size_t cap;
    size_t len;
} DynBuf;

static void dynbuf_append(DynBuf *b, uint8_t byte)
{
    if (b->len >= b->cap)
    {
        size_t new_cap = b->cap ? b->cap * 2 : 65536;
        uint8_t *new_data = (uint8_t *)realloc(b->data, new_cap);
        if (!new_data)
            DIE(PNG_ERR_MEMORY);
        b->data = new_data;
        b->cap = new_cap;
    }
    b->data[b->len++] = byte;
}

/* Decode coded block using literal + distance Huffman tables */
static void inflate_block_codes(BitReader *br, const HuffTable *lit,
                                const HuffTable *dist, DynBuf *out)
{
    for (;;)
    {
        int sym = huff_decode(br, lit);
        if (sym < 256)
        {
            dynbuf_append(out, (uint8_t)sym);
        }
        else if (sym == 256)
        {
            break;
        }
        else
        {
            int li = sym - 257;
            if (li < 0 || li >= 29)
                DIE(PNG_ERR_BAD_DATA);
            int length = length_base[li] + (int)br_read(br, length_extra[li]);
            int di = huff_decode(br, dist);
            if (di < 0 || di >= 30)
                DIE(PNG_ERR_BAD_DATA);
            int distance = dist_base[di] + (int)br_read(br, dist_extra[di]);
            if ((size_t)distance > out->len)
                DIE(PNG_ERR_BAD_DATA);
            for (int k = 0; k < length; k++)
            {
                dynbuf_append(out, out->data[out->len - distance]);
            }
        }
    }
}

static void inflate_fixed_block(BitReader *br, DynBuf *out)
{
    HuffTable lit, dist;
    make_fixed_tables(&lit, &dist);
    inflate_block_codes(br, &lit, &dist, out);
}

static void inflate_dynamic_block(BitReader *br, DynBuf *out)
{
    int hlit = (int)br_read(br, 5) + 257;
    int hdist = (int)br_read(br, 5) + 1;
    int hclen = (int)br_read(br, 4) + 4;

    uint8_t cl_lens[19] = {0};
    for (int i = 0; i < hclen; i++)
        cl_lens[cl_order[i]] = (uint8_t)br_read(br, 3);

    HuffTable cl_table;
    huff_build(&cl_table, cl_lens, 19);

    int total = hlit + hdist;
    uint8_t *all_lens = (uint8_t *)calloc((size_t)total, 1);
    if (!all_lens)
        DIE(PNG_ERR_MEMORY);

    int i = 0;
    while (i < total)
    {
        int s = huff_decode(br, &cl_table);
        if (s < 16)
        {
            all_lens[i++] = (uint8_t)s;
        }
        else if (s == 16)
        {
            if (i == 0)
                DIE(PNG_ERR_BAD_DATA);
            int rep = (int)br_read(br, 2) + 3;
            while (rep-- && i < total)
            {
                all_lens[i] = all_lens[i - 1];
                i++;
            }
        }
        else if (s == 17)
        {
            int rep = (int)br_read(br, 3) + 3;
            while (rep-- && i < total)
                all_lens[i++] = 0;
        }
        else
        {
            int rep = (int)br_read(br, 7) + 11;
            while (rep-- && i < total)
                all_lens[i++] = 0;
        }
    }

    HuffTable lit_t, dist_t;
    huff_build(&lit_t, all_lens, hlit);
    huff_build(&dist_t, all_lens + hlit, hdist);
    free(all_lens);

    inflate_block_codes(br, &lit_t, &dist_t, out);
}

/* Top-level inflate: skip zlib wrapper (2B header + Adler32 trailer) */
static DynBuf inflate_zlib(const uint8_t *zdata, size_t zlen)
{
    if (zlen < 2)
        DIE(PNG_ERR_TRUNCATED);
    /* skip 2-byte zlib header */
    BitReader br;
    br_init(&br, zdata + 2, zlen - 2);

    DynBuf out = {0};
    int bfinal = 0;
    while (!bfinal)
    {
        bfinal = (int)br_read(&br, 1);
        int btype = (int)br_read(&br, 2);

        if (btype == 0)
        {
            /* Stored block */
            br_byte_align(&br);
            uint16_t len = (uint16_t)br_read(&br, 16);
            uint16_t nlen = (uint16_t)br_read(&br, 16);
            if ((uint16_t)(len ^ nlen) != 0xFFFF)
                DIE(PNG_ERR_BAD_DATA);
            for (int k = 0; k < (int)len; k++)
                dynbuf_append(&out, (uint8_t)br_read(&br, 8));
        }
        else if (btype == 1)
        {
            inflate_fixed_block(&br, &out);
        }
        else if (btype == 2)
        {
            inflate_dynamic_block(&br, &out);
        }
        else
        {
            DIE(PNG_ERR_BAD_DATA);
        }
    }
    return out;
}

/* ════════════════════════════════════════════════════════════
   PNG chunk parsing
   ════════════════════════════════════════════════════════════ */
static const uint8_t PNG_SIG[8] = {137, 80, 78, 71, 13, 10, 26, 10};

static inline uint32_t read_u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* Paeth predictor */
static inline uint8_t paeth(int a, int b, int c)
{
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    if (pa <= pb && pa <= pc)
        return (uint8_t)a;
    if (pb <= pc)
        return (uint8_t)b;
    return (uint8_t)c;
}

/* Unfilter a single scanline (in-place on cur) */
static void unfilter_scanline(int filter, uint8_t *cur, const uint8_t *prev,
                              int stride, int bpp)
{
    switch (filter)
    {
    case 0:
        break;
    case 1:
        for (int i = bpp; i < stride; i++)
            cur[i] += cur[i - bpp];
        break;
    case 2:
        for (int i = 0; i < stride; i++)
            cur[i] += prev[i];
        break;
    case 3:
        for (int i = 0; i < stride; i++)
        {
            int a = (i >= bpp) ? cur[i - bpp] : 0;
            int b = prev[i];
            cur[i] += (uint8_t)((a + b) / 2);
        }
        break;
    case 4:
        for (int i = 0; i < stride; i++)
        {
            int a = (i >= bpp) ? cur[i - bpp] : 0;
            int b = prev[i];
            int c = (i >= bpp) ? prev[i - bpp] : 0;
            cur[i] += paeth(a, b, c);
        }
        break;
    default:
        DIE(PNG_ERR_BAD_DATA);
    }
}

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */
int png_decode(const unsigned char *png_data, size_t png_len,
               unsigned char **out_rgba,
               unsigned int *out_width, unsigned int *out_height,
               unsigned int *out_channels)
{
    if (!png_data || png_len < 8)
        return PNG_ERR_TRUNCATED;

    /* setjmp for error recovery */
    g_err_code = PNG_OK;
    if (setjmp(g_err_jmp))
        return g_err_code;

    /* Check signature */
    if (memcmp(png_data, PNG_SIG, 8) != 0)
        DIE(PNG_ERR_SIGNATURE);

    size_t offset = 8;
    uint32_t width = 0, height = 0;
    int bit_depth = 0, color_type = 0;
    int got_ihdr = 0;
    int channels = 0;

    /* Collect IDAT data */
    uint8_t *idat_buf = NULL;
    size_t idat_len = 0;

    while (offset + 8 <= png_len)
    {
        uint32_t chunk_len = read_u32be(png_data + offset);
        offset += 4;
        char type[5];
        memcpy(type, png_data + offset, 4);
        type[4] = '\0';
        offset += 4;

        if (offset + chunk_len > png_len)
            DIE(PNG_ERR_TRUNCATED);

        if (strcmp(type, "IHDR") == 0)
        {
            if (chunk_len < 13)
                DIE(PNG_ERR_BAD_DATA);
            width = read_u32be(png_data + offset);
            height = read_u32be(png_data + offset + 4);
            bit_depth = png_data[offset + 8];
            color_type = png_data[offset + 9];
            got_ihdr = 1;

            if (bit_depth != 8)
                DIE(PNG_ERR_UNSUPPORT);
            switch (color_type)
            {
            case 2:
                channels = 3;
                break;
            case 6:
                channels = 4;
                break;
            default:
                DIE(PNG_ERR_UNSUPPORT);
            }
        }
        else if (strcmp(type, "IDAT") == 0)
        {
            if (!got_ihdr)
                DIE(PNG_ERR_NO_IHDR);
            uint8_t *tmp = (uint8_t *)realloc(idat_buf, idat_len + chunk_len);
            if (!tmp)
                DIE(PNG_ERR_MEMORY);
            idat_buf = tmp;
            memcpy(idat_buf + idat_len, png_data + offset, chunk_len);
            idat_len += chunk_len;
        }
        else if (strcmp(type, "IEND") == 0)
        {
            break;
        }

        offset += chunk_len + 4; /* skip data + CRC */
    }

    if (!got_ihdr)
        DIE(PNG_ERR_NO_IHDR);
    if (!idat_buf)
        DIE(PNG_ERR_NO_IDAT);

    /* Decompress IDAT */
    DynBuf raw = inflate_zlib(idat_buf, idat_len);
    free(idat_buf);

    /* Reconstruct scanlines */
    int bpp = channels;
    int stride = (int)width * bpp;
    size_t expected = (size_t)(stride + 1) * height;
    if (raw.len < expected)
        DIE(PNG_ERR_TRUNCATED);

    uint8_t *pixels = (uint8_t *)malloc((size_t)stride * height);
    if (!pixels)
    {
        free(raw.data);
        DIE(PNG_ERR_MEMORY);
    }

    uint8_t *zero_row = (uint8_t *)calloc(1, (size_t)(stride > 0 ? stride : 1));
    if (!zero_row)
    {
        free(pixels);
        free(raw.data);
        DIE(PNG_ERR_MEMORY);
    }

    for (uint32_t y = 0; y < height; y++)
    {
        uint8_t *src = raw.data + (size_t)y * (stride + 1);
        uint8_t filter = src[0];
        uint8_t *cur = pixels + (size_t)y * stride;
        uint8_t *prev = (y == 0) ? zero_row : pixels + (size_t)(y - 1) * stride;
        memcpy(cur, src + 1, (size_t)stride);
        unfilter_scanline(filter, cur, prev, stride, bpp);
    }

    free(zero_row);
    free(raw.data);

    *out_rgba = pixels;
    *out_width = width;
    *out_height = height;
    *out_channels = (unsigned int)channels;
    return PNG_OK;
}
