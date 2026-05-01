#include "browser_internal.h"

#include <stdio.h>

char ush_browser_ascii_tolower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

int ush_browser_line_has_token_icase(const char *line, u64 line_len, const char *token) {
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

void ush_browser_text_reset(void) {
    ush_browser_text_len = 0ULL;
    ush_browser_text_buf[0] = '\0';
    ush_browser_last_space = 1;
}

void ush_browser_text_trim_trailing_spaces(void) {
    while (ush_browser_text_len > 0ULL && ush_browser_text_buf[ush_browser_text_len - 1ULL] == ' ') {
        ush_browser_text_len--;
    }
    ush_browser_text_buf[ush_browser_text_len] = '\0';
}

void ush_browser_text_newline(void) {
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

void ush_browser_text_append_char(char ch) {
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

void ush_browser_text_append_raw_char(char ch) {
    if (ch == '\0' || ush_browser_text_len + 1ULL >= (u64)USH_BROWSER_TEXT_BUF_CAP) {
        return;
    }

    if ((unsigned char)ch < 0x20U && ch != '\n' && ch != '\t') {
        return;
    }

    ush_browser_text_buf[ush_browser_text_len++] = ch;
    ush_browser_text_buf[ush_browser_text_len] = '\0';
    ush_browser_last_space = (ch == ' ' || ch == '\n' || ch == '\t') ? 1 : 0;
}

void ush_browser_text_append(const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        ush_browser_text_append_char(text[i]);
        i++;
    }
}

void ush_browser_text_append_raw(const char *text) {
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

void ush_browser_text_horizontal_rule(void) {
    ush_browser_text_newline();
    ush_browser_text_append_raw("------------------------------------------------------------");
    ush_browser_text_newline();
}

void ush_browser_style_reset(ush_browser_style *out_style) {
    if (out_style == (ush_browser_style *)0) {
        return;
    }

    out_style->fg_set = 0;
    out_style->fg_rgb = 0U;
    out_style->bg_set = 0;
    out_style->bg_rgb = 0U;
    out_style->bold = 0;
    out_style->italic = 0;
    out_style->dim = 0;
    out_style->underline = 0;
    out_style->strike = 0;
    out_style->display_none = 0;
    out_style->white_space_pre = 0;
    out_style->text_transform = USH_BROWSER_TEXT_TRANSFORM_NONE;
    out_style->font_scale = 1;
}

void ush_browser_style_delta_reset(ush_browser_style_delta *out_delta) {
    if (out_delta == (ush_browser_style_delta *)0) {
        return;
    }

    out_delta->set_fg = 0;
    out_delta->fg_rgb = 0U;
    out_delta->set_bg = 0;
    out_delta->bg_rgb = 0U;
    out_delta->set_bold = 0;
    out_delta->bold = 0;
    out_delta->set_italic = 0;
    out_delta->italic = 0;
    out_delta->set_dim = 0;
    out_delta->dim = 0;
    out_delta->set_underline = 0;
    out_delta->underline = 0;
    out_delta->set_strike = 0;
    out_delta->strike = 0;
    out_delta->set_display_none = 0;
    out_delta->display_none = 0;
    out_delta->set_white_space_pre = 0;
    out_delta->white_space_pre = 0;
    out_delta->set_text_transform = 0;
    out_delta->text_transform = USH_BROWSER_TEXT_TRANSFORM_NONE;
    out_delta->set_font_scale = 0;
    out_delta->font_scale = 1;
}

int ush_browser_style_equal(const ush_browser_style *a, const ush_browser_style *b) {
    if (a == (const ush_browser_style *)0 || b == (const ush_browser_style *)0) {
        return 0;
    }

    return (a->fg_set == b->fg_set && a->fg_rgb == b->fg_rgb && a->bg_set == b->bg_set && a->bg_rgb == b->bg_rgb &&
            a->bold == b->bold && a->italic == b->italic && a->dim == b->dim && a->underline == b->underline &&
            a->strike == b->strike && a->display_none == b->display_none && a->white_space_pre == b->white_space_pre &&
            a->text_transform == b->text_transform && a->font_scale == b->font_scale)
               ? 1
               : 0;
}

void ush_browser_style_apply_delta(ush_browser_style *io_style, const ush_browser_style_delta *delta) {
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
    if (delta->set_italic != 0) {
        io_style->italic = (delta->italic != 0) ? 1 : 0;
    }
    if (delta->set_dim != 0) {
        io_style->dim = (delta->dim != 0) ? 1 : 0;
    }
    if (delta->set_underline != 0) {
        io_style->underline = (delta->underline != 0) ? 1 : 0;
    }
    if (delta->set_strike != 0) {
        io_style->strike = (delta->strike != 0) ? 1 : 0;
    }
    if (delta->set_display_none != 0) {
        io_style->display_none = (delta->display_none != 0) ? 1 : 0;
    }
    if (delta->set_white_space_pre != 0) {
        io_style->white_space_pre = (delta->white_space_pre != 0) ? 1 : 0;
    }
    if (delta->set_text_transform != 0) {
        io_style->text_transform = delta->text_transform;
    }
    if (delta->set_font_scale != 0) {
        io_style->font_scale = delta->font_scale;
        if (io_style->font_scale < 1) {
            io_style->font_scale = 1;
        }
        if (io_style->font_scale > 3) {
            io_style->font_scale = 3;
        }
    }
}

void ush_browser_style_apply_anchor_default(ush_browser_style *io_style) {
    if (io_style == (ush_browser_style *)0) {
        return;
    }

    io_style->fg_set = 1;
    io_style->fg_rgb = 0x0000FFU;
    io_style->underline = 1;
}

void ush_browser_text_emit_style(const ush_browser_style *style) {
    char seq[128];
    u64 at = 0ULL;
    int wrote;
    int font_scale;

    if (style == (const ush_browser_style *)0) {
        return;
    }

    font_scale = style->font_scale;
    if (font_scale < 1) {
        font_scale = 1;
    }
    if (font_scale > 3) {
        font_scale = 3;
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

    if (style->dim != 0) {
        wrote = snprintf(seq + at, sizeof(seq) - at, ";2");
        if (wrote <= 0 || (u64)wrote >= (u64)(sizeof(seq) - at)) {
            ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
            return;
        }
        at += (u64)wrote;
    }

    if (style->italic != 0) {
        wrote = snprintf(seq + at, sizeof(seq) - at, ";3");
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

    if (style->strike != 0) {
        wrote = snprintf(seq + at, sizeof(seq) - at, ";9");
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

    {
        char scale_seq[16];
        wrote = snprintf(scale_seq, sizeof(scale_seq), "\x1B[%dz", font_scale);
        if (wrote > 0 && (u64)wrote < (u64)sizeof(scale_seq)) {
            ush_browser_text_append_raw(scale_seq);
        }
    }
}
