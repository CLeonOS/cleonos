extern "C" {
#include <clks/framebuffer.h>
#include <clks/log.h>
#include <clks/panic_qr.h>
#include <clks/serial.h>
#include <clks/string.h>
#include <clks/types.h>
}

#ifndef NDEBUG
#define NDEBUG 1
#endif

#define MINIZ_NO_STDIO 1
#define MINIZ_NO_INFLATE_APIS 1
#define MINIZ_NO_TIME 1
#define MINIZ_NO_MALLOC 1
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES 1
#include "../../third_party/miniz/miniz.h"

extern "C" {
#include "../../third_party/qrcodegen/qrcodegen.h"
}

#define CLKS_PANIC_QR_BORDER 4U
#define CLKS_PANIC_QR_MAX_LINES 256U
#define CLKS_PANIC_QR_LINE_MAX 256U
#define CLKS_PANIC_QR_TEXT_MAX ((CLKS_PANIC_QR_MAX_LINES * CLKS_PANIC_QR_LINE_MAX) + CLKS_PANIC_QR_MAX_LINES)
#define CLKS_PANIC_QR_COMP_MAX (CLKS_PANIC_QR_TEXT_MAX + 2048U)
#define CLKS_PANIC_QR_MAX_COMP_BYTES 2953U
#define CLKS_PANIC_QR_DIGITS_MAX (((CLKS_PANIC_QR_MAX_COMP_BYTES / 7U) * 17U) + 17U)
#define CLKS_PANIC_QR_URL_PREFIX "https://panic.archlinux.org/panic_report#?a=cleonos-x86_64&v=cleonos&z="
#define CLKS_PANIC_QR_URL_PREFIX_LEN ((u64)(sizeof(CLKS_PANIC_QR_URL_PREFIX) - 1U))

#define CLKS_PANIC_QR_COLOR_DARK 0x00000000U
#define CLKS_PANIC_QR_COLOR_LIGHT 0x00FFFFFFU

static char clks_panic_qr_lines[CLKS_PANIC_QR_MAX_LINES][CLKS_PANIC_QR_LINE_MAX];
static u16 clks_panic_qr_line_len[CLKS_PANIC_QR_MAX_LINES];

static char clks_panic_qr_text[CLKS_PANIC_QR_TEXT_MAX];
static u8 clks_panic_qr_comp[CLKS_PANIC_QR_COMP_MAX];
static char clks_panic_qr_digits[CLKS_PANIC_QR_DIGITS_MAX];

static u8 clks_panic_qr_payload[qrcodegen_BUFFER_LEN_MAX];
static u8 clks_panic_qr_code[qrcodegen_BUFFER_LEN_MAX];
static u8 clks_panic_qr_url_seg_buf[qrcodegen_BUFFER_LEN_MAX];
static u8 clks_panic_qr_num_seg_buf[qrcodegen_BUFFER_LEN_MAX];

static clks_bool clks_panic_qr_ready = CLKS_FALSE;
static clks_bool clks_panic_qr_attempted = CLKS_FALSE;
static u64 clks_panic_qr_total_lines_cache = 0ULL;
static u64 clks_panic_qr_dropped_lines_cache = 0ULL;
static u64 clks_panic_qr_comp_size_cache = 0ULL;
static u64 clks_panic_qr_digits_size_cache = 0ULL;

static u64 clks_panic_qr_collect_lines(void) {
    u64 journal_count = clks_log_journal_count();
    u64 i;
    u64 line_count = 0ULL;

    if (journal_count > CLKS_PANIC_QR_MAX_LINES) {
        journal_count = CLKS_PANIC_QR_MAX_LINES;
    }

    for (i = 0ULL; i < journal_count; i++) {
        if (clks_log_journal_read(i, clks_panic_qr_lines[line_count], CLKS_PANIC_QR_LINE_MAX) == CLKS_TRUE) {
            clks_panic_qr_line_len[line_count] = (u16)clks_strlen(clks_panic_qr_lines[line_count]);
            line_count++;
        }
    }

    return line_count;
}

static u64 clks_panic_qr_build_text(u64 line_start, u64 line_count) {
    u64 i;
    u64 out_len = 0ULL;

    if (line_start >= line_count) {
        return 0ULL;
    }

    for (i = line_start; i < line_count; i++) {
        u64 len = (u64)clks_panic_qr_line_len[i];

        if (out_len + len + 1ULL > CLKS_PANIC_QR_TEXT_MAX) {
            break;
        }

        if (len > 0ULL) {
            clks_memcpy(&clks_panic_qr_text[out_len], clks_panic_qr_lines[i], (usize)len);
            out_len += len;
        }

        clks_panic_qr_text[out_len++] = '\n';
    }

    return out_len;
}

static clks_bool clks_panic_qr_compress(const char *src, u64 src_len, u8 *dst, u64 dst_cap, u64 *out_len) {
    tdefl_compressor comp;
    tdefl_status init_status;
    tdefl_status comp_status;
    size_t in_size;
    size_t out_size;
    mz_uint flags;

    if (src == CLKS_NULL || dst == CLKS_NULL || out_len == CLKS_NULL || src_len == 0ULL || dst_cap == 0ULL) {
        return CLKS_FALSE;
    }

    flags = tdefl_create_comp_flags_from_zip_params(6, 12, MZ_DEFAULT_STRATEGY);
    init_status = tdefl_init(&comp, (tdefl_put_buf_func_ptr)0, (void *)0, (int)flags);
    if (init_status < 0) {
        return CLKS_FALSE;
    }

    in_size = (size_t)src_len;
    out_size = (size_t)dst_cap;
    comp_status = tdefl_compress(&comp, src, &in_size, dst, &out_size, TDEFL_FINISH);

    if (comp_status != TDEFL_STATUS_DONE || in_size != (size_t)src_len) {
        return CLKS_FALSE;
    }

    *out_len = (u64)out_size;
    return CLKS_TRUE;
}

static void clks_panic_qr_u64_to_dec_padded(u64 value, u32 digits, char *out) {
    u32 i;

    if (out == CLKS_NULL || digits == 0U) {
        return;
    }

    for (i = 0U; i < digits; i++) {
        out[digits - 1U - i] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }
}

static u64 clks_panic_qr_binary_to_decimal(const u8 *src, u64 src_len, char *dst, u64 dst_cap) {
    static const u8 tail_digits[7] = {0U, 3U, 5U, 8U, 10U, 13U, 15U};
    u64 in_pos = 0ULL;
    u64 out_pos = 0ULL;

    if (src == CLKS_NULL || dst == CLKS_NULL || dst_cap == 0ULL) {
        return 0ULL;
    }

    while (in_pos + 7ULL <= src_len) {
        u64 value = 0ULL;
        u32 i;

        if (out_pos + 17ULL + 1ULL > dst_cap) {
            return 0ULL;
        }

        for (i = 0U; i < 7U; i++) {
            value |= ((u64)src[in_pos + (u64)i]) << (i * 8U);
        }

        clks_panic_qr_u64_to_dec_padded(value, 17U, &dst[out_pos]);
        out_pos += 17ULL;
        in_pos += 7ULL;
    }

    if (in_pos < src_len) {
        u64 value = 0ULL;
        u64 rem = src_len - in_pos;
        u32 digits = tail_digits[rem];
        u32 i;

        if (digits == 0U || out_pos + (u64)digits + 1ULL > dst_cap) {
            return 0ULL;
        }

        for (i = 0U; i < (u32)rem; i++) {
            value |= ((u64)src[in_pos + (u64)i]) << (i * 8U);
        }

        clks_panic_qr_u64_to_dec_padded(value, digits, &dst[out_pos]);
        out_pos += (u64)digits;
    }

    dst[out_pos] = '\0';
    return out_pos;
}

static clks_bool clks_panic_qr_encode_payload(const u8 *payload, u64 payload_size, u64 *digit_len_out) {
    struct qrcodegen_Segment segs[2];
    u64 digit_len;
    size_t url_seg_buf_len;
    size_t num_seg_buf_len;

    if (payload == CLKS_NULL || payload_size == 0ULL || payload_size > CLKS_PANIC_QR_MAX_COMP_BYTES) {
        return CLKS_FALSE;
    }

    digit_len = clks_panic_qr_binary_to_decimal(payload, payload_size, clks_panic_qr_digits, CLKS_PANIC_QR_DIGITS_MAX);
    if (digit_len == 0ULL) {
        return CLKS_FALSE;
    }

    url_seg_buf_len = qrcodegen_calcSegmentBufferSize(qrcodegen_Mode_BYTE, (size_t)CLKS_PANIC_QR_URL_PREFIX_LEN);
    num_seg_buf_len = qrcodegen_calcSegmentBufferSize(qrcodegen_Mode_NUMERIC, (size_t)digit_len);
    if (url_seg_buf_len == (size_t)-1 || num_seg_buf_len == (size_t)-1 ||
        url_seg_buf_len > qrcodegen_BUFFER_LEN_MAX || num_seg_buf_len > qrcodegen_BUFFER_LEN_MAX) {
        return CLKS_FALSE;
    }

    segs[0] = qrcodegen_makeBytes((const u8 *)CLKS_PANIC_QR_URL_PREFIX, (size_t)CLKS_PANIC_QR_URL_PREFIX_LEN,
                                  clks_panic_qr_url_seg_buf);
    segs[1] = qrcodegen_makeNumeric(clks_panic_qr_digits, clks_panic_qr_num_seg_buf);

    if (qrcodegen_encodeSegmentsAdvanced(segs, 2U, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                                         qrcodegen_Mask_AUTO, true, clks_panic_qr_payload,
                                         clks_panic_qr_code) == false) {
        return CLKS_FALSE;
    }

    if (digit_len_out != CLKS_NULL) {
        *digit_len_out = digit_len;
    }

    return CLKS_TRUE;
}

extern "C" clks_bool clks_panic_qr_prepare(void) {
    u64 line_count;
    u64 start;

    if (clks_panic_qr_attempted == CLKS_TRUE) {
        return clks_panic_qr_ready;
    }

    clks_panic_qr_attempted = CLKS_TRUE;
    clks_panic_qr_ready = CLKS_FALSE;
    clks_panic_qr_total_lines_cache = 0ULL;
    clks_panic_qr_dropped_lines_cache = 0ULL;
    clks_panic_qr_comp_size_cache = 0ULL;
    clks_panic_qr_digits_size_cache = 0ULL;

    line_count = clks_panic_qr_collect_lines();
    clks_panic_qr_total_lines_cache = line_count;

    if (line_count == 0ULL) {
        return CLKS_FALSE;
    }

    for (start = 0ULL; start < line_count; start++) {
        u64 text_len = clks_panic_qr_build_text(start, line_count);
        u64 comp_len = 0ULL;
        u64 digit_len = 0ULL;

        if (text_len == 0ULL) {
            continue;
        }

        if (clks_panic_qr_compress(clks_panic_qr_text, text_len, clks_panic_qr_comp, CLKS_PANIC_QR_COMP_MAX,
                                   &comp_len) == CLKS_FALSE) {
            continue;
        }

        if (comp_len > CLKS_PANIC_QR_MAX_COMP_BYTES) {
            continue;
        }

        if (clks_panic_qr_encode_payload(clks_panic_qr_comp, comp_len, &digit_len) == CLKS_FALSE) {
            continue;
        }

        clks_panic_qr_dropped_lines_cache = start;
        clks_panic_qr_comp_size_cache = comp_len;
        clks_panic_qr_digits_size_cache = digit_len;
        clks_panic_qr_ready = CLKS_TRUE;
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

extern "C" clks_bool clks_panic_qr_show(void) {
    struct clks_framebuffer_info fb_info;
    int qr_size;
    u32 modules;
    u32 scale;
    u32 draw_w;
    u32 draw_h;
    u32 base_x;
    u32 base_y;
    u32 y;
    u32 x;

    if (clks_panic_qr_ready == CLKS_FALSE) {
        if (clks_panic_qr_prepare() == CLKS_FALSE) {
            return CLKS_FALSE;
        }
    }

    if (clks_fb_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    qr_size = qrcodegen_getSize(clks_panic_qr_code);
    if (qr_size <= 0) {
        return CLKS_FALSE;
    }

    fb_info = clks_fb_info();
    modules = (u32)qr_size + (CLKS_PANIC_QR_BORDER * 2U);
    if (modules == 0U) {
        return CLKS_FALSE;
    }

    scale = fb_info.width / modules;
    if ((fb_info.height / modules) < scale) {
        scale = fb_info.height / modules;
    }

    if (scale == 0U) {
        return CLKS_FALSE;
    }

    draw_w = modules * scale;
    draw_h = modules * scale;
    base_x = (fb_info.width > draw_w) ? ((fb_info.width - draw_w) / 2U) : 0U;
    base_y = (fb_info.height > draw_h) ? ((fb_info.height - draw_h) / 2U) : 0U;

    clks_fb_clear(CLKS_PANIC_QR_COLOR_LIGHT);

    for (y = 0U; y < modules; y++) {
        for (x = 0U; x < modules; x++) {
            int qx = (int)x - (int)CLKS_PANIC_QR_BORDER;
            int qy = (int)y - (int)CLKS_PANIC_QR_BORDER;
            clks_bool dark = CLKS_FALSE;

            if (qx >= 0 && qy >= 0 && qx < qr_size && qy < qr_size) {
                dark = qrcodegen_getModule(clks_panic_qr_code, qx, qy) ? CLKS_TRUE : CLKS_FALSE;
            }

            if (dark == CLKS_TRUE) {
                clks_fb_fill_rect(base_x + (x * scale), base_y + (y * scale), scale, scale, CLKS_PANIC_QR_COLOR_DARK);
            }
        }
    }

    clks_serial_write("[PANIC][QR] LINES:");
    {
        char hex_buf[19];
        clks_serial_write(" TOTAL=");
        clks_memset(hex_buf, 0, sizeof(hex_buf));
        hex_buf[0] = '0';
        hex_buf[1] = 'X';
        for (int i = 0; i < 16; i++) {
            u8 n = (u8)((clks_panic_qr_total_lines_cache >> ((15 - i) * 4)) & 0x0FULL);
            hex_buf[2 + i] = (n < 10U) ? (char)('0' + n) : (char)('A' + (n - 10U));
        }
        clks_serial_write(hex_buf);

        clks_serial_write(" DROPPED=");
        for (int i = 0; i < 16; i++) {
            u8 n = (u8)((clks_panic_qr_dropped_lines_cache >> ((15 - i) * 4)) & 0x0FULL);
            hex_buf[2 + i] = (n < 10U) ? (char)('0' + n) : (char)('A' + (n - 10U));
        }
        clks_serial_write(hex_buf);

        clks_serial_write(" COMP_BYTES=");
        for (int i = 0; i < 16; i++) {
            u8 n = (u8)((clks_panic_qr_comp_size_cache >> ((15 - i) * 4)) & 0x0FULL);
            hex_buf[2 + i] = (n < 10U) ? (char)('0' + n) : (char)('A' + (n - 10U));
        }
        clks_serial_write(hex_buf);
        clks_serial_write(" DIGITS=");
        for (int i = 0; i < 16; i++) {
            u8 n = (u8)((clks_panic_qr_digits_size_cache >> ((15 - i) * 4)) & 0x0FULL);
            hex_buf[2 + i] = (n < 10U) ? (char)('0' + n) : (char)('A' + (n - 10U));
        }
        clks_serial_write(hex_buf);
        clks_serial_write("\n");
    }

    return CLKS_TRUE;
}

extern "C" u64 clks_panic_qr_total_lines(void) {
    return clks_panic_qr_total_lines_cache;
}

extern "C" u64 clks_panic_qr_dropped_lines(void) {
    return clks_panic_qr_dropped_lines_cache;
}
