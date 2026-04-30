#include "cmd_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gumbo.h"

#include "browser/browser_shared.h"

static char *ush_browser_html_buf;
char *ush_browser_http_raw_buf;
static char *ush_browser_text_buf;
static char ush_browser_title[USH_BROWSER_TITLE_MAX];
static ush_browser_link ush_browser_links[USH_BROWSER_LINK_MAX];

static u64 ush_browser_text_len = 0ULL;
static int ush_browser_last_space = 1;
static u64 ush_browser_link_count = 0ULL;
static ush_browser_css_rule ush_browser_css_rules[USH_BROWSER_CSS_RULE_MAX];
static u64 ush_browser_css_rule_count = 0ULL;

int ush_browser_ensure_buffers(void) {
    if (ush_browser_html_buf != (char *)0 && ush_browser_http_raw_buf != (char *)0 &&
        ush_browser_text_buf != (char *)0) {
        return 1;
    }

    ush_browser_html_buf = (char *)malloc((size_t)USH_BROWSER_HTML_BUF_CAP);
    ush_browser_http_raw_buf = (char *)malloc((size_t)USH_BROWSER_HTML_BUF_CAP);
    ush_browser_text_buf = (char *)malloc((size_t)USH_BROWSER_TEXT_BUF_CAP);
    if (ush_browser_html_buf == (char *)0 || ush_browser_http_raw_buf == (char *)0 ||
        ush_browser_text_buf == (char *)0) {
        return 0;
    }

    ush_browser_html_buf[0] = '\0';
    ush_browser_http_raw_buf[0] = '\0';
    ush_browser_text_buf[0] = '\0';
    return 1;
}

static char ush_browser_ascii_tolower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static int ush_browser_line_has_token_icase(const char *line, u64 line_len, const char *token) {
    u64 token_len;
    u64 i;

    if (line == (const char *)0 || token == (const char *)0) {
        return 0;
    }

    token_len = ush_strlen(token);
    if (token_len == 0ULL || token_len > line_len) {
        return 0;
    }

    for (i = 0ULL; i + token_len <= line_len; i++) {
        u64 j;
        int match = 1;
        for (j = 0ULL; j < token_len; j++) {
            if (ush_browser_ascii_tolower(line[i + j]) != ush_browser_ascii_tolower(token[j])) {
                match = 0;
                break;
            }
        }
        if (match != 0) {
            return 1;
        }
    }

    return 0;
}

static void ush_browser_text_reset(void) {
    ush_browser_text_len = 0ULL;
    ush_browser_text_buf[0] = '\0';
    ush_browser_last_space = 1;
}

static void ush_browser_text_trim_trailing_spaces(void) {
    while (ush_browser_text_len > 0ULL && ush_browser_text_buf[ush_browser_text_len - 1ULL] == ' ') {
        ush_browser_text_len--;
    }
    ush_browser_text_buf[ush_browser_text_len] = '\0';
}

static void ush_browser_text_newline(void) {
    if (ush_browser_text_len == 0ULL) {
        return;
    }

    ush_browser_text_trim_trailing_spaces();
    if (ush_browser_text_len == 0ULL) {
        return;
    }

    if (ush_browser_text_buf[ush_browser_text_len - 1ULL] == '\n') {
        if (ush_browser_text_len >= 2ULL && ush_browser_text_buf[ush_browser_text_len - 2ULL] == '\n') {
            return;
        }
    }

    if (ush_browser_text_len + 1ULL >= (u64)USH_BROWSER_TEXT_BUF_CAP) {
        return;
    }

    ush_browser_text_buf[ush_browser_text_len++] = '\n';
    ush_browser_text_buf[ush_browser_text_len] = '\0';
    ush_browser_last_space = 1;
}

static void ush_browser_text_append_char(char ch) {
    if (ush_browser_text_len + 1ULL >= (u64)USH_BROWSER_TEXT_BUF_CAP) {
        return;
    }

    if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\v') {
        ch = ' ';
    }

    if ((unsigned char)ch < 0x20U) {
        return;
    }

    if (ch == ' ') {
        if (ush_browser_last_space != 0) {
            return;
        }
        ush_browser_last_space = 1;
    } else {
        ush_browser_last_space = 0;
    }

    ush_browser_text_buf[ush_browser_text_len++] = ch;
    ush_browser_text_buf[ush_browser_text_len] = '\0';
}

static void ush_browser_text_append(const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        ush_browser_text_append_char(text[i]);
        i++;
    }
}

static void ush_browser_text_append_raw(const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        if (ush_browser_text_len + 1ULL >= (u64)USH_BROWSER_TEXT_BUF_CAP) {
            return;
        }

        ush_browser_text_buf[ush_browser_text_len++] = text[i++];
        ush_browser_text_buf[ush_browser_text_len] = '\0';
    }
}

static void ush_browser_style_reset(ush_browser_style *out_style) {
    if (out_style == (ush_browser_style *)0) {
        return;
    }

    out_style->fg_set = 0;
    out_style->fg_rgb = 0U;
    out_style->bg_set = 0;
    out_style->bg_rgb = 0U;
    out_style->bold = 0;
    out_style->underline = 0;
    out_style->display_none = 0;
}

static void ush_browser_style_delta_reset(ush_browser_style_delta *out_delta) {
    if (out_delta == (ush_browser_style_delta *)0) {
        return;
    }

    out_delta->set_fg = 0;
    out_delta->fg_rgb = 0U;
    out_delta->set_bg = 0;
    out_delta->bg_rgb = 0U;
    out_delta->set_bold = 0;
    out_delta->bold = 0;
    out_delta->set_underline = 0;
    out_delta->underline = 0;
    out_delta->set_display_none = 0;
    out_delta->display_none = 0;
}

static int ush_browser_style_equal(const ush_browser_style *a, const ush_browser_style *b) {
    if (a == (const ush_browser_style *)0 || b == (const ush_browser_style *)0) {
        return 0;
    }

    return (a->fg_set == b->fg_set && a->fg_rgb == b->fg_rgb && a->bg_set == b->bg_set && a->bg_rgb == b->bg_rgb &&
            a->bold == b->bold && a->underline == b->underline && a->display_none == b->display_none)
               ? 1
               : 0;
}

static void ush_browser_style_apply_delta(ush_browser_style *io_style, const ush_browser_style_delta *delta) {
    if (io_style == (ush_browser_style *)0 || delta == (const ush_browser_style_delta *)0) {
        return;
    }

    if (delta->set_fg != 0) {
        io_style->fg_set = 1;
        io_style->fg_rgb = delta->fg_rgb;
    }
    if (delta->set_bg != 0) {
        io_style->bg_set = 1;
        io_style->bg_rgb = delta->bg_rgb;
    }
    if (delta->set_bold != 0) {
        io_style->bold = (delta->bold != 0) ? 1 : 0;
    }
    if (delta->set_underline != 0) {
        io_style->underline = (delta->underline != 0) ? 1 : 0;
    }
    if (delta->set_display_none != 0) {
        io_style->display_none = (delta->display_none != 0) ? 1 : 0;
    }
}

static void ush_browser_style_apply_anchor_default(ush_browser_style *io_style) {
    if (io_style == (ush_browser_style *)0) {
        return;
    }

    io_style->fg_set = 1;
    io_style->fg_rgb = 0x0000FFU;
    io_style->underline = 1;
}

static void ush_browser_text_emit_style(const ush_browser_style *style) {
    char seq[128];
    u64 at = 0ULL;
    int wrote;

    if (style == (const ush_browser_style *)0) {
        return;
    }

    wrote = snprintf(seq, sizeof(seq), "\x1B[0");
    if (wrote <= 0 || (u64)wrote >= (u64)sizeof(seq)) {
        ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
        return;
    }
    at = (u64)wrote;

    if (style->bold != 0) {
        wrote = snprintf(seq + at, sizeof(seq) - at, ";1");
        if (wrote <= 0 || (u64)wrote >= (u64)(sizeof(seq) - at)) {
            ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
            return;
        }
        at += (u64)wrote;
    }

    if (style->underline != 0) {
        wrote = snprintf(seq + at, sizeof(seq) - at, ";4");
        if (wrote <= 0 || (u64)wrote >= (u64)(sizeof(seq) - at)) {
            ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
            return;
        }
        at += (u64)wrote;
    }

    if (style->fg_set != 0) {
        u32 r = (style->fg_rgb >> 16U) & 0xFFU;
        u32 g = (style->fg_rgb >> 8U) & 0xFFU;
        u32 b = style->fg_rgb & 0xFFU;
        wrote =
            snprintf(seq + at, sizeof(seq) - at, ";38;2;%u;%u;%u", (unsigned int)r, (unsigned int)g, (unsigned int)b);
        if (wrote <= 0 || (u64)wrote >= (u64)(sizeof(seq) - at)) {
            ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
            return;
        }
        at += (u64)wrote;
    }

    if (style->bg_set != 0) {
        u32 r = (style->bg_rgb >> 16U) & 0xFFU;
        u32 g = (style->bg_rgb >> 8U) & 0xFFU;
        u32 b = style->bg_rgb & 0xFFU;
        wrote =
            snprintf(seq + at, sizeof(seq) - at, ";48;2;%u;%u;%u", (unsigned int)r, (unsigned int)g, (unsigned int)b);
        if (wrote <= 0 || (u64)wrote >= (u64)(sizeof(seq) - at)) {
            ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
            return;
        }
        at += (u64)wrote;
    }

    wrote = snprintf(seq + at, sizeof(seq) - at, "m");
    if (wrote <= 0 || (u64)wrote >= (u64)(sizeof(seq) - at)) {
        ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
        return;
    }

    ush_browser_text_append_raw(seq);
}

static int ush_browser_css_is_space(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v') ? 1 : 0;
}

static int ush_browser_css_name_eq(const char *name, u64 name_len, const char *lit) {
    u64 lit_len = ush_strlen(lit);
    u64 i;

    if (name == (const char *)0 || lit == (const char *)0 || name_len != lit_len) {
        return 0;
    }

    for (i = 0ULL; i < lit_len; i++) {
        if (ush_browser_ascii_tolower(name[i]) != ush_browser_ascii_tolower(lit[i])) {
            return 0;
        }
    }

    return 1;
}

static int ush_browser_css_hex_nibble(char ch, u32 *out_value) {
    if (out_value == (u32 *)0) {
        return 0;
    }

    if (ch >= '0' && ch <= '9') {
        *out_value = (u32)(ch - '0');
        return 1;
    }
    if (ch >= 'a' && ch <= 'f') {
        *out_value = (u32)(ch - 'a') + 10U;
        return 1;
    }
    if (ch >= 'A' && ch <= 'F') {
        *out_value = (u32)(ch - 'A') + 10U;
        return 1;
    }
    return 0;
}

static int ush_browser_css_parse_u32_dec(const char *text, u64 len, u64 *io_pos, u32 *out_value) {
    u64 pos;
    u64 acc = 0ULL;
    int has_digit = 0;

    if (text == (const char *)0 || io_pos == (u64 *)0 || out_value == (u32 *)0 || *io_pos >= len) {
        return 0;
    }

    pos = *io_pos;
    while (pos < len && text[pos] >= '0' && text[pos] <= '9') {
        acc = acc * 10ULL + (u64)(text[pos] - '0');
        if (acc > 0xFFFFFFFFULL) {
            return 0;
        }
        has_digit = 1;
        pos++;
    }

    if (has_digit == 0) {
        return 0;
    }

    *io_pos = pos;
    *out_value = (u32)acc;
    return 1;
}

static int ush_browser_css_parse_color(const char *value, u64 value_len, u32 *out_rgb) {
    char normalized[96];
    u64 i;
    u64 at = 0ULL;

    if (value == (const char *)0 || out_rgb == (u32 *)0) {
        return 0;
    }

    for (i = 0ULL; i < value_len; i++) {
        char ch = value[i];
        if (ush_browser_css_is_space(ch) != 0) {
            continue;
        }
        if (at + 1ULL >= (u64)sizeof(normalized)) {
            break;
        }
        normalized[at++] = ush_browser_ascii_tolower(ch);
    }
    normalized[at] = '\0';

    if (at == 0ULL) {
        return 0;
    }

    if (normalized[0] == '#') {
        if (at == 4ULL) {
            u32 r0;
            u32 g0;
            u32 b0;
            if (ush_browser_css_hex_nibble(normalized[1], &r0) == 0 ||
                ush_browser_css_hex_nibble(normalized[2], &g0) == 0 ||
                ush_browser_css_hex_nibble(normalized[3], &b0) == 0) {
                return 0;
            }
            *out_rgb = ((r0 << 20U) | (r0 << 16U) | (g0 << 12U) | (g0 << 8U) | (b0 << 4U) | b0);
            return 1;
        }
        if (at == 7ULL) {
            u32 n[6];
            for (i = 0ULL; i < 6ULL; i++) {
                if (ush_browser_css_hex_nibble(normalized[i + 1ULL], &n[i]) == 0) {
                    return 0;
                }
            }
            *out_rgb = ((n[0] << 20U) | (n[1] << 16U) | (n[2] << 12U) | (n[3] << 8U) | (n[4] << 4U) | n[5]);
            return 1;
        }
    }

    if (at >= 6ULL && normalized[0] == 'r' && normalized[1] == 'g' && normalized[2] == 'b' && normalized[3] == '(' &&
        normalized[at - 1ULL] == ')') {
        u64 pos = 4ULL;
        u32 r;
        u32 g;
        u32 b;

        if (ush_browser_css_parse_u32_dec(normalized, at - 1ULL, &pos, &r) == 0 || pos >= at - 1ULL ||
            normalized[pos] != ',' || r > 255U) {
            return 0;
        }
        pos++;
        if (ush_browser_css_parse_u32_dec(normalized, at - 1ULL, &pos, &g) == 0 || pos >= at - 1ULL ||
            normalized[pos] != ',' || g > 255U) {
            return 0;
        }
        pos++;
        if (ush_browser_css_parse_u32_dec(normalized, at - 1ULL, &pos, &b) == 0 || b > 255U) {
            return 0;
        }
        *out_rgb = (r << 16U) | (g << 8U) | b;
        return 1;
    }

    if (ush_streq(normalized, "black") != 0) {
        *out_rgb = 0x000000U;
        return 1;
    }
    if (ush_streq(normalized, "white") != 0) {
        *out_rgb = 0xFFFFFFU;
        return 1;
    }
    if (ush_streq(normalized, "red") != 0) {
        *out_rgb = 0xFF0000U;
        return 1;
    }
    if (ush_streq(normalized, "green") != 0) {
        *out_rgb = 0x008000U;
        return 1;
    }
    if (ush_streq(normalized, "blue") != 0) {
        *out_rgb = 0x0000FFU;
        return 1;
    }
    if (ush_streq(normalized, "yellow") != 0) {
        *out_rgb = 0xFFFF00U;
        return 1;
    }
    if (ush_streq(normalized, "magenta") != 0 || ush_streq(normalized, "fuchsia") != 0) {
        *out_rgb = 0xFF00FFU;
        return 1;
    }
    if (ush_streq(normalized, "cyan") != 0 || ush_streq(normalized, "aqua") != 0) {
        *out_rgb = 0x00FFFFU;
        return 1;
    }
    if (ush_streq(normalized, "gray") != 0 || ush_streq(normalized, "grey") != 0) {
        *out_rgb = 0x808080U;
        return 1;
    }
    if (ush_streq(normalized, "silver") != 0) {
        *out_rgb = 0xC0C0C0U;
        return 1;
    }
    if (ush_streq(normalized, "orange") != 0) {
        *out_rgb = 0xFFA500U;
        return 1;
    }
    if (ush_streq(normalized, "purple") != 0) {
        *out_rgb = 0x800080U;
        return 1;
    }
    if (ush_streq(normalized, "navy") != 0) {
        *out_rgb = 0x000080U;
        return 1;
    }
    if (ush_streq(normalized, "teal") != 0) {
        *out_rgb = 0x008080U;
        return 1;
    }
    if (ush_streq(normalized, "lime") != 0) {
        *out_rgb = 0x00FF00U;
        return 1;
    }
    if (ush_streq(normalized, "maroon") != 0) {
        *out_rgb = 0x800000U;
        return 1;
    }
    if (ush_streq(normalized, "olive") != 0) {
        *out_rgb = 0x808000U;
        return 1;
    }

    return 0;
}

static void ush_browser_css_apply_declarations(const char *decl, u64 decl_len, ush_browser_style_delta *io_delta) {
    u64 pos = 0ULL;

    if (decl == (const char *)0 || io_delta == (ush_browser_style_delta *)0) {
        return;
    }

    while (pos < decl_len) {
        u64 name_start;
        u64 name_end;
        u64 value_start;
        u64 value_end;
        u32 rgb;

        while (pos < decl_len && (ush_browser_css_is_space(decl[pos]) != 0 || decl[pos] == ';')) {
            pos++;
        }
        if (pos >= decl_len) {
            break;
        }

        name_start = pos;
        while (pos < decl_len && decl[pos] != ':' && decl[pos] != ';' && decl[pos] != '}') {
            pos++;
        }
        name_end = pos;
        while (name_end > name_start && ush_browser_css_is_space(decl[name_end - 1ULL]) != 0) {
            name_end--;
        }

        if (pos >= decl_len || decl[pos] != ':') {
            while (pos < decl_len && decl[pos] != ';') {
                pos++;
            }
            if (pos < decl_len) {
                pos++;
            }
            continue;
        }

        pos++;
        value_start = pos;
        while (pos < decl_len && decl[pos] != ';' && decl[pos] != '}') {
            pos++;
        }
        value_end = pos;
        while (value_start < value_end && ush_browser_css_is_space(decl[value_start]) != 0) {
            value_start++;
        }
        while (value_end > value_start && ush_browser_css_is_space(decl[value_end - 1ULL]) != 0) {
            value_end--;
        }

        if (name_end > name_start && value_end >= value_start) {
            const char *value = decl + value_start;
            u64 value_len = value_end - value_start;

            if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "text-decoration") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "none") != 0) {
                    io_delta->set_underline = 1;
                    io_delta->underline = 0;
                }
                if (ush_browser_line_has_token_icase(value, value_len, "underline") != 0) {
                    io_delta->set_underline = 1;
                    io_delta->underline = 1;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "color") != 0) {
                if (ush_browser_css_parse_color(value, value_len, &rgb) != 0) {
                    io_delta->set_fg = 1;
                    io_delta->fg_rgb = rgb;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "background-color") != 0) {
                if (ush_browser_css_parse_color(value, value_len, &rgb) != 0) {
                    io_delta->set_bg = 1;
                    io_delta->bg_rgb = rgb;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "font-weight") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "bold") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "600") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "700") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "800") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "900") != 0) {
                    io_delta->set_bold = 1;
                    io_delta->bold = 1;
                } else if (ush_browser_line_has_token_icase(value, value_len, "normal") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "100") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "200") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "300") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "400") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "500") != 0) {
                    io_delta->set_bold = 1;
                    io_delta->bold = 0;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "display") != 0) {
                io_delta->set_display_none = 1;
                io_delta->display_none = (ush_browser_line_has_token_icase(value, value_len, "none") != 0) ? 1 : 0;
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "visibility") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "hidden") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 1;
                } else if (ush_browser_line_has_token_icase(value, value_len, "visible") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 0;
                }
            }
        }

        if (pos < decl_len && decl[pos] == ';') {
            pos++;
        }
    }
}

static int ush_browser_css_is_ident_char(char ch) {
    return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')
               ? 1
               : 0;
}

static int ush_browser_css_capture_ident(const char *text, u64 len, u64 *io_pos, char *out, u64 out_cap) {
    u64 pos;
    u64 at = 0ULL;

    if (text == (const char *)0 || io_pos == (u64 *)0 || out == (char *)0 || out_cap == 0ULL || *io_pos >= len) {
        return 0;
    }

    pos = *io_pos;
    if (ush_browser_css_is_ident_char(text[pos]) == 0) {
        return 0;
    }

    while (pos < len && ush_browser_css_is_ident_char(text[pos]) != 0) {
        if (at + 1ULL >= out_cap) {
            return 0;
        }
        out[at++] = ush_browser_ascii_tolower(text[pos]);
        pos++;
    }

    out[at] = '\0';
    *io_pos = pos;
    return (at > 0ULL) ? 1 : 0;
}

static int ush_browser_css_parse_simple_selector(const char *selector, u64 selector_len,
                                                 ush_browser_css_rule *out_rule) {
    u64 pos = 0ULL;
    int matched = 0;

    if (selector == (const char *)0 || out_rule == (ush_browser_css_rule *)0 || selector_len == 0ULL) {
        return 0;
    }

    ush_zero(out_rule, (u64)sizeof(*out_rule));

    while (pos < selector_len && ush_browser_css_is_space(selector[pos]) != 0) {
        pos++;
    }

    while (pos < selector_len) {
        char ch = selector[pos];
        if (ch == ':' || ch == '[') {
            break;
        }
        if (ch == '#') {
            pos++;
            if (out_rule->id_name[0] == '\0' &&
                ush_browser_css_capture_ident(selector, selector_len, &pos, out_rule->id_name,
                                              (u64)sizeof(out_rule->id_name)) != 0) {
                matched = 1;
                continue;
            }
            while (pos < selector_len && ush_browser_css_is_ident_char(selector[pos]) != 0) {
                pos++;
            }
            continue;
        }
        if (ch == '.') {
            pos++;
            if (out_rule->class_name[0] == '\0' &&
                ush_browser_css_capture_ident(selector, selector_len, &pos, out_rule->class_name,
                                              (u64)sizeof(out_rule->class_name)) != 0) {
                matched = 1;
                continue;
            }
            while (pos < selector_len && ush_browser_css_is_ident_char(selector[pos]) != 0) {
                pos++;
            }
            continue;
        }
        if (ush_browser_css_is_ident_char(ch) != 0) {
            if (out_rule->tag[0] == '\0' && ush_browser_css_capture_ident(selector, selector_len, &pos, out_rule->tag,
                                                                          (u64)sizeof(out_rule->tag)) != 0) {
                matched = 1;
                continue;
            }
            while (pos < selector_len && ush_browser_css_is_ident_char(selector[pos]) != 0) {
                pos++;
            }
            continue;
        }
        if (ch == '*') {
            matched = 1;
            pos++;
            continue;
        }

        pos++;
    }

    return matched;
}

static void ush_browser_css_add_rule(const ush_browser_css_rule *rule) {
    if (rule == (const ush_browser_css_rule *)0 || ush_browser_css_rule_count >= (u64)USH_BROWSER_CSS_RULE_MAX) {
        return;
    }

    ush_browser_css_rules[ush_browser_css_rule_count++] = *rule;
}

static int ush_browser_css_is_selector_separator(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '>' || ch == '+' || ch == '~') ? 1 : 0;
}

static void ush_browser_css_parse_selector_list(const char *selector_text, u64 selector_len,
                                                const ush_browser_style_delta *delta) {
    u64 part_start = 0ULL;
    u64 i;

    if (selector_text == (const char *)0 || delta == (const ush_browser_style_delta *)0) {
        return;
    }

    for (i = 0ULL; i <= selector_len; i++) {
        if (i == selector_len || selector_text[i] == ',') {
            u64 start = part_start;
            u64 end = i;
            u64 compound_start;
            u64 compound_end;
            ush_browser_css_rule rule;

            while (start < end && ush_browser_css_is_space(selector_text[start]) != 0) {
                start++;
            }
            while (end > start && ush_browser_css_is_space(selector_text[end - 1ULL]) != 0) {
                end--;
            }

            if (end > start) {
                compound_end = end;
                compound_start = compound_end;

                while (compound_start > start &&
                       ush_browser_css_is_selector_separator(selector_text[compound_start - 1ULL]) == 0) {
                    compound_start--;
                }
                while (compound_start < compound_end &&
                       ush_browser_css_is_selector_separator(selector_text[compound_start]) != 0) {
                    compound_start++;
                }

                if (compound_end > compound_start &&
                    ush_browser_css_parse_simple_selector(selector_text + compound_start, compound_end - compound_start,
                                                          &rule) != 0) {
                    rule.delta = *delta;
                    ush_browser_css_add_rule(&rule);
                }
            }

            part_start = i + 1ULL;
        }
    }
}

static void ush_browser_css_skip_ws_and_comments(const char *text, u64 len, u64 *io_pos) {
    u64 pos;

    if (text == (const char *)0 || io_pos == (u64 *)0 || *io_pos > len) {
        return;
    }

    pos = *io_pos;
    for (;;) {
        while (pos < len && ush_browser_css_is_space(text[pos]) != 0) {
            pos++;
        }
        if (pos + 1ULL < len && text[pos] == '/' && text[pos + 1ULL] == '*') {
            pos += 2ULL;
            while (pos + 1ULL < len && !(text[pos] == '*' && text[pos + 1ULL] == '/')) {
                pos++;
            }
            if (pos + 1ULL < len) {
                pos += 2ULL;
            } else {
                pos = len;
            }
            continue;
        }
        break;
    }

    *io_pos = pos;
}

static void ush_browser_css_parse_stylesheet(const char *css_text) {
    u64 pos = 0ULL;
    u64 css_len;

    if (css_text == (const char *)0) {
        return;
    }

    css_len = ush_strlen(css_text);
    while (pos < css_len) {
        u64 selector_start;
        u64 selector_end;
        u64 decl_start;
        u64 decl_end;
        ush_browser_style_delta delta;

        ush_browser_css_skip_ws_and_comments(css_text, css_len, &pos);
        if (pos >= css_len) {
            break;
        }

        selector_start = pos;
        while (pos < css_len && css_text[pos] != '{') {
            pos++;
        }
        if (pos >= css_len) {
            break;
        }
        selector_end = pos;
        pos++;

        decl_start = pos;
        while (pos < css_len && css_text[pos] != '}') {
            pos++;
        }
        decl_end = pos;

        ush_browser_style_delta_reset(&delta);
        ush_browser_css_apply_declarations(css_text + decl_start, decl_end - decl_start, &delta);
        ush_browser_css_parse_selector_list(css_text + selector_start, selector_end - selector_start, &delta);

        if (pos < css_len && css_text[pos] == '}') {
            pos++;
        }
    }
}

static void ush_browser_collect_style_text(GumboNode *node, char *out, u64 out_cap, u64 *io_len) {
    if (node == (GumboNode *)0 || out == (char *)0 || io_len == (u64 *)0 || out_cap == 0ULL) {
        return;
    }

    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
        const char *txt = node->v.text.text;
        u64 i = 0ULL;

        while (txt != (const char *)0 && txt[i] != '\0') {
            if (*io_len + 1ULL >= out_cap) {
                break;
            }
            out[(*io_len)++] = txt[i++];
        }
        out[*io_len] = '\0';
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children =
            (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_collect_style_text((GumboNode *)children->data[i], out, out_cap, io_len);
        }
    }
}

static void ush_browser_css_scan_style_nodes(GumboNode *node) {
    if (node == (GumboNode *)0) {
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag == GUMBO_TAG_STYLE) {
        char css_text[USH_BROWSER_CSS_TEXT_MAX];
        u64 css_len = 0ULL;

        ush_zero(css_text, (u64)sizeof(css_text));
        ush_browser_collect_style_text(node, css_text, (u64)sizeof(css_text), &css_len);
        if (css_len > 0ULL) {
            ush_browser_css_parse_stylesheet(css_text);
        }
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children =
            (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_css_scan_style_nodes((GumboNode *)children->data[i]);
        }
    }
}

static int ush_browser_css_attr_eq_icase(const char *left, const char *right) {
    u64 i = 0ULL;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (ush_browser_ascii_tolower(left[i]) != ush_browser_ascii_tolower(right[i])) {
            return 0;
        }
        i++;
    }

    return (left[i] == '\0' && right[i] == '\0') ? 1 : 0;
}

static int ush_browser_css_class_contains(const char *classes, const char *needle) {
    u64 pos = 0ULL;
    u64 needle_len;

    if (classes == (const char *)0 || needle == (const char *)0 || needle[0] == '\0') {
        return 0;
    }

    needle_len = ush_strlen(needle);
    while (classes[pos] != '\0') {
        u64 start;
        u64 end;
        u64 len;
        u64 i;
        int same = 1;

        while (classes[pos] != '\0' && ush_browser_css_is_space(classes[pos]) != 0) {
            pos++;
        }
        if (classes[pos] == '\0') {
            break;
        }

        start = pos;
        while (classes[pos] != '\0' && ush_browser_css_is_space(classes[pos]) == 0) {
            pos++;
        }
        end = pos;
        len = end - start;
        if (len != needle_len) {
            continue;
        }

        for (i = 0ULL; i < len; i++) {
            if (ush_browser_ascii_tolower(classes[start + i]) != ush_browser_ascii_tolower(needle[i])) {
                same = 0;
                break;
            }
        }

        if (same != 0) {
            return 1;
        }
    }

    return 0;
}

static int ush_browser_css_rule_matches_node(const ush_browser_css_rule *rule, GumboNode *node) {
    const char *tag_name;
    GumboAttribute *class_attr;
    GumboAttribute *id_attr;

    if (rule == (const ush_browser_css_rule *)0 || node == (GumboNode *)0 || node->type != GUMBO_NODE_ELEMENT) {
        return 0;
    }

    tag_name = gumbo_normalized_tagname(node->v.element.tag);
    if (rule->tag[0] != '\0') {
        if (tag_name == (const char *)0 || ush_browser_css_attr_eq_icase(tag_name, rule->tag) == 0) {
            return 0;
        }
    }

    if (rule->class_name[0] != '\0') {
        class_attr = gumbo_get_attribute(&node->v.element.attributes, "class");
        if (class_attr == (GumboAttribute *)0 || class_attr->value == (const char *)0 ||
            ush_browser_css_class_contains(class_attr->value, rule->class_name) == 0) {
            return 0;
        }
    }

    if (rule->id_name[0] != '\0') {
        id_attr = gumbo_get_attribute(&node->v.element.attributes, "id");
        if (id_attr == (GumboAttribute *)0 || id_attr->value == (const char *)0 ||
            ush_browser_css_attr_eq_icase(id_attr->value, rule->id_name) == 0) {
            return 0;
        }
    }

    return 1;
}

static void ush_browser_css_apply_rules_for_node(GumboNode *node, ush_browser_style *io_style) {
    u64 i;

    if (node == (GumboNode *)0 || io_style == (ush_browser_style *)0 || node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    for (i = 0ULL; i < ush_browser_css_rule_count; i++) {
        if (ush_browser_css_rule_matches_node(&ush_browser_css_rules[i], node) != 0) {
            ush_browser_style_apply_delta(io_style, &ush_browser_css_rules[i].delta);
        }
    }
}

static int ush_browser_is_skip_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_SCRIPT:
    case GUMBO_TAG_STYLE:
    case GUMBO_TAG_NOSCRIPT:
    case GUMBO_TAG_TEMPLATE:
    case GUMBO_TAG_IFRAME:
    case GUMBO_TAG_OBJECT:
        return 1;
    default:
        return 0;
    }
}

static int ush_browser_is_block_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_HTML:
    case GUMBO_TAG_HEAD:
    case GUMBO_TAG_BODY:
    case GUMBO_TAG_DIV:
    case GUMBO_TAG_SECTION:
    case GUMBO_TAG_ARTICLE:
    case GUMBO_TAG_ASIDE:
    case GUMBO_TAG_NAV:
    case GUMBO_TAG_ADDRESS:
    case GUMBO_TAG_P:
    case GUMBO_TAG_UL:
    case GUMBO_TAG_OL:
    case GUMBO_TAG_LI:
    case GUMBO_TAG_DL:
    case GUMBO_TAG_DT:
    case GUMBO_TAG_DD:
    case GUMBO_TAG_TABLE:
    case GUMBO_TAG_CAPTION:
    case GUMBO_TAG_TBODY:
    case GUMBO_TAG_THEAD:
    case GUMBO_TAG_TFOOT:
    case GUMBO_TAG_TR:
    case GUMBO_TAG_TD:
    case GUMBO_TAG_TH:
    case GUMBO_TAG_PRE:
    case GUMBO_TAG_BLOCKQUOTE:
    case GUMBO_TAG_FIGURE:
    case GUMBO_TAG_FIGCAPTION:
    case GUMBO_TAG_H1:
    case GUMBO_TAG_H2:
    case GUMBO_TAG_H3:
    case GUMBO_TAG_H4:
    case GUMBO_TAG_H5:
    case GUMBO_TAG_H6:
    case GUMBO_TAG_FOOTER:
    case GUMBO_TAG_HEADER:
    case GUMBO_TAG_MAIN:
    case GUMBO_TAG_HGROUP:
    case GUMBO_TAG_FORM:
    case GUMBO_TAG_FIELDSET:
    case GUMBO_TAG_DETAILS:
    case GUMBO_TAG_SUMMARY:
    case GUMBO_TAG_MENU:
    case GUMBO_TAG_DIR:
    case GUMBO_TAG_LISTING:
    case GUMBO_TAG_XMP:
    case GUMBO_TAG_PLAINTEXT:
    case GUMBO_TAG_CENTER:
    case GUMBO_TAG_DIALOG:
    case GUMBO_TAG_SEARCH:
        return 1;
    default:
        return 0;
    }
}

static void ush_browser_collect_plain_text(GumboNode *node, char *out, u64 out_cap, u64 *io_len) {
    if (node == (GumboNode *)0 || out == (char *)0 || io_len == (u64 *)0 || out_cap == 0ULL) {
        return;
    }

    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
        const char *txt = node->v.text.text;
        u64 i = 0ULL;

        while (txt != (const char *)0 && txt[i] != '\0') {
            char ch = txt[i];

            if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\v') {
                ch = ' ';
            }

            if ((unsigned char)ch >= 0x20U) {
                if (*io_len + 1ULL >= out_cap) {
                    break;
                }

                if (ch == ' ') {
                    if (*io_len > 0ULL && out[*io_len - 1ULL] != ' ') {
                        out[(*io_len)++] = ' ';
                    }
                } else {
                    out[(*io_len)++] = ch;
                }
            }

            i++;
        }

        out[*io_len] = '\0';
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        GumboVector *children = &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_collect_plain_text((GumboNode *)children->data[i], out, out_cap, io_len);
        }
    }
}

static void ush_browser_collect_anchor_link(GumboNode *node) {
    GumboAttribute *href;
    u64 text_len = 0ULL;
    char text[USH_BROWSER_LINK_TEXT_MAX];

    if (node == (GumboNode *)0 || node->type != GUMBO_NODE_ELEMENT || node->v.element.tag != GUMBO_TAG_A) {
        return;
    }

    href = gumbo_get_attribute(&node->v.element.attributes, "href");
    if (href == (GumboAttribute *)0 || href->value == (const char *)0 || href->value[0] == '\0') {
        return;
    }

    if (ush_browser_link_count >= (u64)USH_BROWSER_LINK_MAX) {
        return;
    }

    ush_zero(text, (u64)sizeof(text));
    ush_browser_collect_plain_text(node, text, (u64)sizeof(text), &text_len);
    if (text[0] == '\0') {
        ush_copy(text, (u64)sizeof(text), "(link)");
    }

    ush_copy(ush_browser_links[ush_browser_link_count].text,
             (u64)sizeof(ush_browser_links[ush_browser_link_count].text), text);
    ush_copy(ush_browser_links[ush_browser_link_count].href,
             (u64)sizeof(ush_browser_links[ush_browser_link_count].href), href->value);
    ush_browser_link_count++;
}

static int ush_browser_find_title_node(GumboNode *node, char *out, u64 out_cap) {
    if (node == (GumboNode *)0 || out == (char *)0 || out_cap == 0ULL) {
        return 0;
    }

    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag == GUMBO_TAG_TITLE) {
        u64 len = 0ULL;
        out[0] = '\0';
        ush_browser_collect_plain_text(node, out, out_cap, &len);
        return (out[0] != '\0') ? 1 : 0;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children =
            (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            if (ush_browser_find_title_node((GumboNode *)children->data[i], out, out_cap) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static void ush_browser_walk_dom_styled(GumboNode *node, const ush_browser_style *parent_style) {
    if (node == (GumboNode *)0 || parent_style == (const ush_browser_style *)0) {
        return;
    }

    switch (node->type) {
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_WHITESPACE:
    case GUMBO_NODE_CDATA:
        if (parent_style->display_none == 0) {
            ush_browser_text_append(node->v.text.text);
        }
        return;

    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE: {
        GumboTag tag = node->v.element.tag;
        GumboVector *children = &node->v.element.children;
        GumboAttribute *style_attr;
        ush_browser_style style = *parent_style;
        ush_browser_style_delta inline_delta;
        int is_block = ush_browser_is_block_tag(tag);
        int style_changed;
        u64 i;

        if (ush_browser_is_skip_tag(tag) != 0) {
            return;
        }

        if (tag == GUMBO_TAG_A) {
            ush_browser_collect_anchor_link(node);
            ush_browser_style_apply_anchor_default(&style);
        }

        ush_browser_css_apply_rules_for_node(node, &style);

        style_attr = gumbo_get_attribute(&node->v.element.attributes, "style");
        if (style_attr != (GumboAttribute *)0 && style_attr->value != (const char *)0) {
            ush_browser_style_delta_reset(&inline_delta);
            ush_browser_css_apply_declarations(style_attr->value, ush_strlen(style_attr->value), &inline_delta);
            ush_browser_style_apply_delta(&style, &inline_delta);
        }

        if (style.display_none != 0) {
            return;
        }

        if (tag == GUMBO_TAG_BR || tag == GUMBO_TAG_HR) {
            ush_browser_text_newline();
            return;
        }

        style_changed = (ush_browser_style_equal(&style, parent_style) == 0) ? 1 : 0;

        if (is_block != 0) {
            ush_browser_text_newline();
            if (tag == GUMBO_TAG_LI) {
                ush_browser_text_append("* ");
            }
        }

        if (style_changed != 0) {
            ush_browser_text_emit_style(&style);
        }

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom_styled((GumboNode *)children->data[i], &style);
        }

        if (style_changed != 0) {
            ush_browser_text_emit_style(parent_style);
        }

        if (is_block != 0) {
            ush_browser_text_newline();
        }
        return;
    }

    case GUMBO_NODE_DOCUMENT: {
        GumboVector *children = &node->v.document.children;
        u64 i;

        if (parent_style->display_none != 0) {
            return;
        }

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom_styled((GumboNode *)children->data[i], parent_style);
        }
        return;
    }

    default:
        return;
    }
}

static int ush_browser_read_file(const ush_state *sh, const char *arg, char *out_html, u64 out_html_cap,
                                 u64 *out_size) {
    char abs_path[USH_PATH_MAX];
    u64 fd;
    u64 total = 0ULL;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0 ||
        out_html_cap == 0ULL) {
        return 0;
    }

    if (ush_resolve_path(sh, arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_stat_type(abs_path) != 1ULL) {
        return 0;
    }

    fd = cleonos_sys_fd_open(abs_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        return 0;
    }

    while (total + 1ULL < out_html_cap) {
        u64 got = cleonos_sys_fd_read(fd, out_html + total, out_html_cap - 1ULL - total);

        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        total += got;
    }

    (void)cleonos_sys_fd_close(fd);
    out_html[total] = '\0';
    *out_size = total;
    return (total > 0ULL) ? 1 : 0;
}

static int ush_browser_render_html_fallback(const char *html, u64 html_size) {
    u64 i = 0ULL;
    int in_tag = 0;
    int pending_space = 0;

    if (html == (const char *)0 || html_size == 0ULL) {
        return 0;
    }

    ush_browser_link_count = 0ULL;
    ush_browser_css_rule_count = 0ULL;
    ush_browser_text_reset();
    ush_zero(ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_copy(ush_browser_title, (u64)sizeof(ush_browser_title), "HTML fallback");

    while (i < html_size && html[i] != '\0') {
        char ch = html[i];

        if (ch == '<') {
            in_tag = 1;
            if (pending_space == 0) {
                ush_browser_text_append_char(' ');
                pending_space = 1;
            }
            i++;
            continue;
        }

        if (in_tag != 0) {
            if (ch == '>') {
                in_tag = 0;
            }
            i++;
            continue;
        }

        if (ch == '&') {
            ush_browser_text_append_char(' ');
            pending_space = 1;
            i++;
            continue;
        }

        ush_browser_text_append_char(ch);
        pending_space = (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') ? 1 : 0;
        i++;
    }

    ush_browser_text_trim_trailing_spaces();
    if (ush_browser_text_len == 0ULL) {
        ush_browser_text_append("[browser] no renderable text");
    }

    return 1;
}

static int ush_browser_render_html(const char *html, u64 html_size) {
    GumboOutput *output;
    GumboOptions options;
    ush_browser_style root_style;
    u64 parse_size;

    if (html == (const char *)0 || html_size == 0ULL) {
        return 0;
    }

    parse_size = html_size;
    if (parse_size > (u64)USH_BROWSER_GUMBO_PARSE_MAX) {
        parse_size = (u64)USH_BROWSER_GUMBO_PARSE_MAX;
        return ush_browser_render_html_fallback(html, parse_size);
    }

    options = kGumboDefaultOptions;
    options.max_errors = USH_BROWSER_GUMBO_MAX_ERRORS;
    options.stop_on_first_error = false;

    output = gumbo_parse_with_options(&options, html, (size_t)parse_size);
    if (output == (GumboOutput *)0 || output->root == (GumboNode *)0) {
        if (output != (GumboOutput *)0) {
            gumbo_destroy_output(&options, output);
        }
        return ush_browser_render_html_fallback(html, parse_size);
    }

    ush_browser_link_count = 0ULL;
    ush_browser_css_rule_count = 0ULL;
    ush_browser_text_reset();
    ush_zero(ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_style_reset(&root_style);

    (void)ush_browser_find_title_node(output->root, ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_css_scan_style_nodes(output->root);
    ush_browser_walk_dom_styled(output->root, &root_style);
    ush_browser_text_trim_trailing_spaces();

    gumbo_destroy_output(&options, output);
    return 1;
}

static int ush_browser_ansi_csi_len(const char *text, u64 pos, u64 len, u64 *out_len) {
    u64 i;

    if (text == (const char *)0 || out_len == (u64 *)0 || pos + 1ULL >= len || text[pos] != '\x1B' ||
        text[pos + 1ULL] != '[') {
        return 0;
    }

    i = pos + 2ULL;
    while (i < len) {
        unsigned char ch = (unsigned char)text[i];
        i++;
        if (ch >= 0x40U && ch <= 0x7EU) {
            *out_len = i - pos;
            return 1;
        }
    }

    return 0;
}

static void ush_browser_write_span(const char *text, u64 start, u64 end) {
    u64 i;

    if (text == (const char *)0 || end <= start) {
        return;
    }

    for (i = start; i < end; i++) {
        (void)fputc(text[i], 1);
    }
    (void)fputc('\n', 1);
}

static int ush_browser_print_wrapped_span(const char *text, u64 start, u64 end, u64 *io_line_count) {
    u64 pos = start;

    if (text == (const char *)0 || io_line_count == (u64 *)0 || end <= start) {
        return 1;
    }

    while (pos < end) {
        u64 line_start;
        u64 line_end;
        u64 last_space = (u64)-1;
        u64 col = 0ULL;

        while (pos < end && text[pos] == ' ') {
            pos++;
        }
        if (pos >= end) {
            break;
        }

        line_start = pos;
        line_end = pos;

        while (pos < end) {
            u64 ansi_len;
            char ch = text[pos];

            if (ush_browser_ansi_csi_len(text, pos, end, &ansi_len) != 0) {
                pos += ansi_len;
                line_end = pos;
                continue;
            }

            if (ch == ' ') {
                last_space = pos;
            }

            if (col >= (u64)USH_BROWSER_OUTPUT_COLS) {
                if (last_space != (u64)-1 && last_space > line_start) {
                    line_end = last_space;
                    pos = last_space + 1ULL;
                }
                break;
            }

            col++;
            pos++;
            line_end = pos;
        }

        while (line_end > line_start && text[line_end - 1ULL] == ' ') {
            line_end--;
        }

        if (line_end > line_start) {
            ush_browser_write_span(text, line_start, line_end);
            (*io_line_count)++;
            if (*io_line_count >= 220ULL) {
                ush_writeln("[browser] output truncated at 220 lines");
                return 0;
            }
        } else if (pos < end) {
            pos++;
        }
    }

    return 1;
}

static void ush_browser_print_rendered(const char *source_desc) {
    u64 i = 0ULL;
    u64 line_count = 0ULL;
    u64 k;

    /* 3J clears TTY scrollback, 2J clears screen, H moves cursor home. */
    ush_writeln("\x1B[3J\x1B[2J\x1B[H");
    ush_writeln("[browser] litehtml-gumbo text renderer");
    (void)printf("[browser] source: %s\n", source_desc);
    if (ush_browser_title[0] != '\0') {
        (void)printf("[browser] title: %s\n", ush_browser_title);
    } else {
        ush_writeln("[browser] title: (none)");
    }
    ush_writeln("------------------------------------------------------------");

    while (ush_browser_text_buf[i] != '\0') {
        u64 start = i;

        while (ush_browser_text_buf[i] != '\0' && ush_browser_text_buf[i] != '\n') {
            i++;
        }

        if (i > start) {
            if (ush_browser_print_wrapped_span(ush_browser_text_buf, start, i, &line_count) == 0) {
                break;
            }
        } else if (line_count > 0ULL) {
            ush_writeln("");
        }

        if (ush_browser_text_buf[i] == '\n') {
            i++;
        }
    }

    if (ush_browser_link_count > 0ULL) {
        ush_writeln("");
        ush_writeln("[links]");
        for (k = 0ULL; k < ush_browser_link_count; k++) {
            (void)printf("  [%llu] " USH_BROWSER_ANSI_BLUE_UNDERLINE "%s" USH_BROWSER_ANSI_RESET
                         " -> " USH_BROWSER_ANSI_BLUE_UNDERLINE "%s" USH_BROWSER_ANSI_RESET "\n",
                         (unsigned long long)(k + 1ULL), ush_browser_links[k].text, ush_browser_links[k].href);
        }
    }
}

static int ush_browser_read_line(char *out_text, u64 out_size) {
    u64 len = 0ULL;

    if (out_text == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_text[0] = '\0';
    for (;;) {
        int ch = fgetc(0);

        if (ch == EOF) {
            return 0;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            (void)fputc('\n', 1);
            out_text[len] = '\0';
            return 1;
        }

        if (ch == 8 || ch == 127) {
            if (len > 0ULL) {
                len--;
                (void)fputs("\b \b", 1);
            }
            continue;
        }

        if ((unsigned int)ch < 0x20U) {
            continue;
        }

        if (len + 1ULL < out_size) {
            out_text[len++] = (char)ch;
            out_text[len] = '\0';
            (void)fputc(ch, 1);
        }
    }
}

static int ush_browser_strip_fragment(const char *in_text, char *out_text, u64 out_size) {
    u64 i = 0ULL;

    if (in_text == (const char *)0 || out_text == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    while (in_text[i] != '\0' && in_text[i] != '#') {
        if (i + 1ULL >= out_size) {
            return 0;
        }
        out_text[i] = in_text[i];
        i++;
    }

    out_text[i] = '\0';
    return 1;
}

static int ush_browser_copy_until_query_or_fragment(const char *in_text, char *out_text, u64 out_size) {
    u64 i = 0ULL;

    if (in_text == (const char *)0 || out_text == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    while (in_text[i] != '\0' && in_text[i] != '?' && in_text[i] != '#') {
        if (i + 1ULL >= out_size) {
            return 0;
        }
        out_text[i] = in_text[i];
        i++;
    }

    out_text[i] = '\0';
    return 1;
}

static int ush_browser_normalize_slash_path(const char *path, char *out_path, u64 out_size) {
    char segments[USH_BROWSER_SEG_MAX][USH_BROWSER_SEG_LEN_MAX + 1U];
    u64 seg_count = 0ULL;
    u64 i = 0ULL;
    u64 out_off = 0ULL;

    if (path == (const char *)0 || out_path == (char *)0 || out_size < 2ULL) {
        return 0;
    }

    while (path[i] != '\0') {
        while (path[i] == '/') {
            i++;
        }

        if (path[i] == '\0') {
            break;
        }

        {
            u64 start = i;
            u64 len = 0ULL;

            while (path[i] != '\0' && path[i] != '/') {
                i++;
            }
            len = i - start;

            if (len == 1ULL && path[start] == '.') {
                continue;
            }

            if (len == 2ULL && path[start] == '.' && path[start + 1ULL] == '.') {
                if (seg_count > 0ULL) {
                    seg_count--;
                }
                continue;
            }

            if (len == 0ULL || len > (u64)USH_BROWSER_SEG_LEN_MAX || seg_count >= (u64)USH_BROWSER_SEG_MAX) {
                return 0;
            }

            (void)memcpy(segments[seg_count], path + start, (usize)len);
            segments[seg_count][len] = '\0';
            seg_count++;
        }
    }

    if (out_size < 2ULL) {
        return 0;
    }

    out_path[out_off++] = '/';
    if (seg_count == 0ULL) {
        out_path[out_off] = '\0';
        return 1;
    }

    for (i = 0ULL; i < seg_count; i++) {
        u64 len = ush_strlen(segments[i]);

        if (out_off + len + 1ULL >= out_size) {
            return 0;
        }

        (void)memcpy(out_path + out_off, segments[i], (usize)len);
        out_off += len;

        if (i + 1ULL < seg_count) {
            out_path[out_off++] = '/';
        }
    }

    out_path[out_off] = '\0';
    return 1;
}

static int ush_browser_join_relative_path(const char *base_path, const char *href_path, char *out_path, u64 out_size) {
    char joined[USH_BROWSER_PATH_MAX * 2U];
    char base_clean[USH_BROWSER_PATH_MAX];
    const char *slash;

    if (base_path == (const char *)0 || href_path == (const char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (href_path[0] == '/') {
        return ush_browser_normalize_slash_path(href_path, out_path, out_size);
    }

    if (ush_browser_copy_until_query_or_fragment(base_path, base_clean, (u64)sizeof(base_clean)) == 0) {
        return 0;
    }

    slash = strrchr(base_clean, '/');
    if (slash == (const char *)0) {
        if (snprintf(joined, sizeof(joined), "/%s", href_path) <= 0) {
            return 0;
        }
    } else {
        u64 dir_len = (u64)(slash - base_clean + 1);

        if (dir_len + ush_strlen(href_path) + 1ULL > (u64)sizeof(joined)) {
            return 0;
        }
        (void)memcpy(joined, base_clean, (usize)dir_len);
        joined[dir_len] = '\0';
        (void)snprintf(joined + dir_len, sizeof(joined) - (usize)dir_len, "%s", href_path);
    }

    return ush_browser_normalize_slash_path(joined, out_path, out_size);
}

static int ush_browser_resolve_http_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    ush_browser_url base;
    char href_no_frag[USH_BROWSER_LINK_HREF_MAX];
    char resolved_path[USH_BROWSER_PATH_MAX];
    const char *scheme;
    u16 default_port;

    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_strip_fragment(href, href_no_frag, (u64)sizeof(href_no_frag)) == 0) {
        return 0;
    }

    if (href_no_frag[0] == '\0' || href_no_frag[0] == '#') {
        ush_copy(out_source, out_size, base_source);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_is_http_url(href_no_frag) != 0 || ush_browser_is_https_url(href_no_frag) != 0) {
        ush_copy(out_source, out_size, href_no_frag);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_parse_url(base_source, &base) == 0) {
        return 0;
    }
    scheme = (base.tls != 0) ? "https" : "http";
    default_port = (base.tls != 0) ? 443U : 80U;

    if (href_no_frag[0] == '/' && href_no_frag[1] == '/') {
        if (snprintf(out_source, (unsigned long)out_size, "%s:%s", scheme, href_no_frag) <= 0) {
            return 0;
        }
        return 1;
    }

    if (href_no_frag[0] == '?') {
        char base_path_only[USH_BROWSER_PATH_MAX];

        if (ush_browser_copy_until_query_or_fragment(base.path, base_path_only, (u64)sizeof(base_path_only)) == 0) {
            return 0;
        }

        if (base.port == default_port) {
            if (snprintf(out_source, (unsigned long)out_size, "%s://%s%s%s", scheme, base.host, base_path_only,
                         href_no_frag) <= 0) {
                return 0;
            }
        } else {
            if (snprintf(out_source, (unsigned long)out_size, "%s://%s:%u%s%s", scheme, base.host,
                         (unsigned int)base.port, base_path_only, href_no_frag) <= 0) {
                return 0;
            }
        }
        return 1;
    }

    if (ush_browser_join_relative_path(base.path, href_no_frag, resolved_path, (u64)sizeof(resolved_path)) == 0) {
        return 0;
    }

    if (base.port == default_port) {
        if (snprintf(out_source, (unsigned long)out_size, "%s://%s%s", scheme, base.host, resolved_path) <= 0) {
            return 0;
        }
    } else {
        if (snprintf(out_source, (unsigned long)out_size, "%s://%s:%u%s", scheme, base.host, (unsigned int)base.port,
                     resolved_path) <= 0) {
            return 0;
        }
    }

    return 1;
}

static int ush_browser_resolve_local_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    char href_no_frag[USH_BROWSER_LINK_HREF_MAX];
    char base_abs[USH_PATH_MAX];
    char resolved[USH_PATH_MAX];

    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_strip_fragment(href, href_no_frag, (u64)sizeof(href_no_frag)) == 0) {
        return 0;
    }

    if (href_no_frag[0] == '\0' || href_no_frag[0] == '#') {
        ush_copy(out_source, out_size, base_source);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (href_no_frag[0] == '/') {
        if (ush_browser_normalize_slash_path(href_no_frag, resolved, (u64)sizeof(resolved)) == 0) {
            return 0;
        }
        ush_copy(out_source, out_size, resolved);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_normalize_slash_path(base_source, base_abs, (u64)sizeof(base_abs)) == 0) {
        ush_copy(base_abs, (u64)sizeof(base_abs), base_source);
    }

    if (href_no_frag[0] == '?') {
        ush_copy(out_source, out_size, base_abs);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_join_relative_path(base_abs, href_no_frag, resolved, (u64)sizeof(resolved)) == 0) {
        return 0;
    }

    ush_copy(out_source, out_size, resolved);
    return (out_source[0] != '\0') ? 1 : 0;
}

static int ush_browser_resolve_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_is_http_url(href) != 0 || ush_browser_is_https_url(href) != 0) {
        ush_copy(out_source, out_size, href);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_is_http_url(base_source) != 0 || ush_browser_is_https_url(base_source) != 0) {
        return ush_browser_resolve_http_href(base_source, href, out_source, out_size);
    }

    return ush_browser_resolve_local_href(base_source, href, out_source, out_size);
}

static int ush_browser_load_source(const ush_state *sh, const char *source, char *out_html, u64 out_html_cap,
                                   u64 *out_size) {
    if (sh == (const ush_state *)0 || source == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0) {
        return 0;
    }

    if (ush_browser_is_http_url(source) != 0 || ush_browser_is_https_url(source) != 0) {
        if (cleonos_sys_net_available() == 0ULL) {
            return 0;
        }
        return ush_browser_fetch_http(source, out_html, out_html_cap, out_size);
    }

    return ush_browser_read_file(sh, source, out_html, out_html_cap, out_size);
}

static int ush_browser_push_history(char history[][USH_BROWSER_SOURCE_MAX], u64 *io_count, const char *source) {
    u64 i;

    if (history == (char(*)[USH_BROWSER_SOURCE_MAX])0 || io_count == (u64 *)0 || source == (const char *)0 ||
        source[0] == '\0') {
        return 0;
    }

    if (*io_count > 0ULL && ush_streq(history[*io_count - 1ULL], source) != 0) {
        return 1;
    }

    if (*io_count >= (u64)USH_BROWSER_HISTORY_MAX) {
        for (i = 1ULL; i < (u64)USH_BROWSER_HISTORY_MAX; i++) {
            ush_copy(history[i - 1ULL], (u64)USH_BROWSER_SOURCE_MAX, history[i]);
        }
        *io_count = (u64)USH_BROWSER_HISTORY_MAX - 1ULL;
    }

    ush_copy(history[*io_count], (u64)USH_BROWSER_SOURCE_MAX, source);
    (*io_count)++;
    return 1;
}

static int ush_browser_pop_history(char history[][USH_BROWSER_SOURCE_MAX], u64 *io_count, char *out_source,
                                   u64 out_size) {
    if (history == (char(*)[USH_BROWSER_SOURCE_MAX])0 || io_count == (u64 *)0 || out_source == (char *)0 ||
        out_size == 0ULL) {
        return 0;
    }

    if (*io_count == 0ULL) {
        return 0;
    }

    (*io_count)--;
    ush_copy(out_source, out_size, history[*io_count]);
    history[*io_count][0] = '\0';
    return 1;
}

static int ush_browser_parse_link_index(const char *text, u64 *out_index) {
    u64 i = 0ULL;
    u64 value = 0ULL;

    if (text == (const char *)0 || out_index == (u64 *)0 || text[0] == '\0') {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10ULL + (u64)(text[i] - '0');
        i++;
    }

    if (value == 0ULL) {
        return 0;
    }

    *out_index = value - 1ULL;
    return 1;
}

/* return: 0 fail, 1 ok, 2 help */
static int ush_browser_parse_args(const char *arg, char *out_source, u64 out_source_size) {
    char first[USH_PATH_MAX];
    const char *rest = "";

    if (out_source == (char *)0 || out_source_size == 0ULL) {
        return 0;
    }

    out_source[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    ush_copy(out_source, out_source_size, first);
    return (out_source[0] != '\0') ? 1 : 0;
}

static void ush_browser_usage(void) {
    ush_writeln("usage: browser <file.html|http://...|https://...>");
    ush_writeln("note: parser is gumbo from litehtml (no handwritten parser)");
}

static int ush_cmd_browser(const ush_state *sh, const char *arg) {
    char source[USH_BROWSER_SOURCE_MAX];
    char current_source[USH_BROWSER_SOURCE_MAX];
    char next_source[USH_BROWSER_SOURCE_MAX];
    char input_line[USH_BROWSER_INPUT_MAX];
    char history[USH_BROWSER_HISTORY_MAX][USH_BROWSER_SOURCE_MAX];
    u64 history_count = 0ULL;
    int parse_ret;
    u64 html_size = 0ULL;
    int loaded_once = 0;
    u64 i;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_browser_ensure_buffers() == 0) {
        ush_writeln("browser: memory allocation failed");
        return 0;
    }

    for (i = 0ULL; i < (u64)USH_BROWSER_HISTORY_MAX; i++) {
        history[i][0] = '\0';
    }

    parse_ret = ush_browser_parse_args(arg, source, (u64)sizeof(source));
    if (parse_ret == 2) {
        ush_browser_usage();
        return 1;
    }

    if (parse_ret == 0) {
        ush_browser_usage();
        return 0;
    }

    if (ush_browser_is_http_url(source) != 0 || ush_browser_is_https_url(source) != 0) {
        ush_copy(current_source, (u64)sizeof(current_source), source);
    } else {
        if (ush_resolve_path(sh, source, current_source, (u64)sizeof(current_source)) == 0) {
            ush_copy(current_source, (u64)sizeof(current_source), source);
        }
    }

    for (;;) {
        if (ush_browser_load_source(sh, current_source, ush_browser_html_buf, (u64)USH_BROWSER_HTML_BUF_CAP,
                                    &html_size) == 0) {
            if (ush_browser_is_http_url(current_source) != 0 || ush_browser_is_https_url(current_source) != 0) {
                (void)printf("browser: URL fetch failed: %s\n", ush_browser_fetch_last_error());
            } else {
                ush_writeln("browser: file read failed");
            }

            if (loaded_once == 0 ||
                ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                return 0;
            }
            ush_writeln("browser: returned to previous page");
            continue;
        }

        if (ush_browser_render_html(ush_browser_html_buf, html_size) == 0) {
            ush_writeln("browser: parse/render failed");
            if (loaded_once == 0 ||
                ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                return 0;
            }
            ush_writeln("browser: returned to previous page");
            continue;
        }

        loaded_once = 1;
        ush_browser_print_rendered(current_source);
        ush_writeln("");
        ush_writeln("[browser] interactive mode");
        ush_writeln("[browser] <number>: open link   o <src>: open source   b: back   r: reload   q: quit");
        (void)fputs("browser> ", 1);

        if (ush_browser_read_line(input_line, (u64)sizeof(input_line)) == 0) {
            return 1;
        }
        ush_trim_line(input_line);

        if (input_line[0] == '\0') {
            continue;
        }

        if (ush_streq(input_line, "q") != 0 || ush_streq(input_line, "quit") != 0 ||
            ush_streq(input_line, "exit") != 0) {
            return 1;
        }

        if (ush_streq(input_line, "r") != 0 || ush_streq(input_line, "reload") != 0) {
            continue;
        }

        if (ush_streq(input_line, "b") != 0 || ush_streq(input_line, "back") != 0) {
            if (ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                ush_writeln("browser: no history");
            }
            continue;
        }

        if (ush_streq(input_line, "h") != 0 || ush_streq(input_line, "help") != 0) {
            ush_writeln("[browser] commands:");
            ush_writeln("  <number>      open link by index");
            ush_writeln("  o <src>       open new URL/path");
            ush_writeln("  b             back");
            ush_writeln("  r             reload");
            ush_writeln("  q             quit");
            continue;
        }

        ush_zero(next_source, (u64)sizeof(next_source));
        if ((input_line[0] == 'o' && input_line[1] == ' ') ||
            (input_line[0] == 'o' && input_line[1] == 'p' && input_line[2] == 'e' && input_line[3] == 'n' &&
             input_line[4] == ' ')) {
            const char *payload = (input_line[1] == ' ') ? (input_line + 2) : (input_line + 5);
            while (*payload == ' ') {
                payload++;
            }
            ush_copy(next_source, (u64)sizeof(next_source), payload);
        } else {
            u64 link_index = 0ULL;
            if (ush_browser_parse_link_index(input_line, &link_index) != 0) {
                if (link_index >= ush_browser_link_count) {
                    ush_writeln("browser: link index out of range");
                    continue;
                }

                if (ush_browser_resolve_href(current_source, ush_browser_links[link_index].href, next_source,
                                             (u64)sizeof(next_source)) == 0) {
                    ush_writeln("browser: cannot resolve link target");
                    continue;
                }
            } else {
                ush_copy(next_source, (u64)sizeof(next_source), input_line);
            }
        }

        if (next_source[0] == '\0') {
            ush_writeln("browser: empty target");
            continue;
        }

        if (ush_browser_is_http_url(next_source) == 0 && ush_browser_is_https_url(next_source) == 0 &&
            next_source[0] != '/') {
            char resolved_target[USH_BROWSER_SOURCE_MAX];

            if (ush_browser_resolve_href(current_source, next_source, resolved_target, (u64)sizeof(resolved_target)) !=
                0) {
                ush_copy(next_source, (u64)sizeof(next_source), resolved_target);
            } else if (ush_resolve_path(sh, next_source, resolved_target, (u64)sizeof(resolved_target)) != 0) {
                ush_copy(next_source, (u64)sizeof(next_source), resolved_target);
            }
        }

        if (ush_streq(next_source, current_source) != 0) {
            continue;
        }

        (void)ush_browser_push_history(history, &history_count, current_source);
        ush_copy(current_source, (u64)sizeof(current_source), next_source);
    }
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    char argv_buf[USH_BROWSER_SOURCE_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";
    u64 argc = 0ULL;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_zero(argv_buf, (u64)sizeof(argv_buf));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "browser") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
            }
        }
    }

    if (has_context == 0) {
        argc = cleonos_sys_proc_argc();
        if (argc > 1ULL) {
            (void)cleonos_sys_proc_argv(1ULL, argv_buf, (u64)sizeof(argv_buf));
            arg = argv_buf;
        }
    }

    success = ush_cmd_browser(&sh, arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
