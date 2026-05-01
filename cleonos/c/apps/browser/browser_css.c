#include "browser_internal.h"

#include "gumbo.h"

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

static int ush_browser_css_parse_color_component(const char *text, u64 len, u64 *io_pos, u32 *out_value) {
    u32 value;

    if (text == (const char *)0 || io_pos == (u64 *)0 || out_value == (u32 *)0) {
        return 0;
    }

    if (ush_browser_css_parse_u32_dec(text, len, io_pos, &value) == 0 || value > 255U) {
        return 0;
    }

    if (*io_pos < len && text[*io_pos] == '%') {
        *io_pos = *io_pos + 1ULL;
        if (value > 100U) {
            value = 100U;
        }
        value = (value * 255U) / 100U;
    }

    *out_value = value;
    return 1;
}

static int ush_browser_css_value_is_keyword(const char *value, u64 value_len, const char *keyword) {
    u64 keyword_len;
    u64 start = 0ULL;
    u64 end = value_len;
    u64 i;

    if (value == (const char *)0 || keyword == (const char *)0) {
        return 0;
    }

    while (start < end && ush_browser_css_is_space(value[start]) != 0) {
        start++;
    }
    while (end > start && ush_browser_css_is_space(value[end - 1ULL]) != 0) {
        end--;
    }

    keyword_len = ush_strlen(keyword);
    if (end - start != keyword_len) {
        return 0;
    }

    for (i = 0ULL; i < keyword_len; i++) {
        if (ush_browser_ascii_tolower(value[start + i]) != ush_browser_ascii_tolower(keyword[i])) {
            return 0;
        }
    }

    return 1;
}

static int ush_browser_css_parse_first_color_token(const char *value, u64 value_len, u32 *out_rgb) {
    u64 pos = 0ULL;

    if (value == (const char *)0 || out_rgb == (u32 *)0) {
        return 0;
    }

    while (pos < value_len) {
        u64 start;
        u64 end;

        while (pos < value_len && (ush_browser_css_is_space(value[pos]) != 0 || value[pos] == ',')) {
            pos++;
        }
        if (pos >= value_len) {
            break;
        }

        start = pos;
        if (value[pos] == '#') {
            pos++;
            while (pos < value_len && ((value[pos] >= '0' && value[pos] <= '9') ||
                                       (value[pos] >= 'a' && value[pos] <= 'f') ||
                                       (value[pos] >= 'A' && value[pos] <= 'F'))) {
                pos++;
            }
        } else {
            while (pos < value_len && ush_browser_css_is_space(value[pos]) == 0 && value[pos] != ',') {
                if (value[pos] == '(') {
                    while (pos < value_len && value[pos] != ')') {
                        pos++;
                    }
                    if (pos < value_len) {
                        pos++;
                    }
                    continue;
                }
                pos++;
            }
        }
        end = pos;

        if (end > start && ush_browser_css_parse_color_value(value + start, end - start, out_rgb) != 0) {
            return 1;
        }
    }

    return 0;
}

int ush_browser_css_parse_color_value(const char *value, u64 value_len, u32 *out_rgb) {
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

    if (((at >= 6ULL && normalized[0] == 'r' && normalized[1] == 'g' && normalized[2] == 'b' &&
          normalized[3] == '(') ||
         (at >= 7ULL && normalized[0] == 'r' && normalized[1] == 'g' && normalized[2] == 'b' &&
          normalized[3] == 'a' && normalized[4] == '(')) &&
        normalized[at - 1ULL] == ')') {
        u64 pos = (normalized[3] == 'a') ? 5ULL : 4ULL;
        u32 r;
        u32 g;
        u32 b;

        if (ush_browser_css_parse_color_component(normalized, at - 1ULL, &pos, &r) == 0 || pos >= at - 1ULL ||
            (normalized[pos] != ',' && normalized[pos] != '/')) {
            return 0;
        }
        pos++;
        if (ush_browser_css_parse_color_component(normalized, at - 1ULL, &pos, &g) == 0 || pos >= at - 1ULL ||
            (normalized[pos] != ',' && normalized[pos] != '/')) {
            return 0;
        }
        pos++;
        if (ush_browser_css_parse_color_component(normalized, at - 1ULL, &pos, &b) == 0) {
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
    if (ush_streq(normalized, "transparent") != 0 || ush_streq(normalized, "currentcolor") != 0) {
        return 0;
    }
    if (ush_streq(normalized, "rebeccapurple") != 0) {
        *out_rgb = 0x663399U;
        return 1;
    }
    if (ush_streq(normalized, "pink") != 0) {
        *out_rgb = 0xFFC0CBU;
        return 1;
    }
    if (ush_streq(normalized, "brown") != 0) {
        *out_rgb = 0xA52A2AU;
        return 1;
    }
    if (ush_streq(normalized, "gold") != 0) {
        *out_rgb = 0xFFD700U;
        return 1;
    }
    if (ush_streq(normalized, "indigo") != 0) {
        *out_rgb = 0x4B0082U;
        return 1;
    }
    if (ush_streq(normalized, "violet") != 0) {
        *out_rgb = 0xEE82EEU;
        return 1;
    }
    if (ush_streq(normalized, "coral") != 0) {
        *out_rgb = 0xFF7F50U;
        return 1;
    }
    if (ush_streq(normalized, "salmon") != 0) {
        *out_rgb = 0xFA8072U;
        return 1;
    }
    if (ush_streq(normalized, "khaki") != 0) {
        *out_rgb = 0xF0E68CU;
        return 1;
    }
    if (ush_streq(normalized, "beige") != 0) {
        *out_rgb = 0xF5F5DCU;
        return 1;
    }
    if (ush_streq(normalized, "tan") != 0) {
        *out_rgb = 0xD2B48CU;
        return 1;
    }
    if (ush_streq(normalized, "plum") != 0) {
        *out_rgb = 0xDDA0DDU;
        return 1;
    }
    if (ush_streq(normalized, "tomato") != 0) {
        *out_rgb = 0xFF6347U;
        return 1;
    }
    if (ush_streq(normalized, "crimson") != 0) {
        *out_rgb = 0xDC143CU;
        return 1;
    }
    if (ush_streq(normalized, "darkgreen") != 0) {
        *out_rgb = 0x006400U;
        return 1;
    }
    if (ush_streq(normalized, "lightgray") != 0 || ush_streq(normalized, "lightgrey") != 0) {
        *out_rgb = 0xD3D3D3U;
        return 1;
    }
    if (ush_streq(normalized, "darkgray") != 0 || ush_streq(normalized, "darkgrey") != 0) {
        *out_rgb = 0xA9A9A9U;
        return 1;
    }

    return 0;
}

void ush_browser_css_apply_declarations(const char *decl, u64 decl_len, ush_browser_style_delta *io_delta) {
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

            if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "text-decoration") != 0 ||
                ush_browser_css_name_eq(decl + name_start, name_end - name_start, "text-decoration-line") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "none") != 0) {
                    io_delta->set_underline = 1;
                    io_delta->underline = 0;
                    io_delta->set_strike = 1;
                    io_delta->strike = 0;
                }
                if (ush_browser_line_has_token_icase(value, value_len, "underline") != 0) {
                    io_delta->set_underline = 1;
                    io_delta->underline = 1;
                }
                if (ush_browser_line_has_token_icase(value, value_len, "line-through") != 0) {
                    io_delta->set_strike = 1;
                    io_delta->strike = 1;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "color") != 0) {
                if (ush_browser_css_parse_color_value(value, value_len, &rgb) != 0) {
                    io_delta->set_fg = 1;
                    io_delta->fg_rgb = rgb;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "background-color") != 0 ||
                       ush_browser_css_name_eq(decl + name_start, name_end - name_start, "background") != 0) {
                if (ush_browser_css_parse_first_color_token(value, value_len, &rgb) != 0) {
                    io_delta->set_bg = 1;
                    io_delta->bg_rgb = rgb;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "border-color") != 0 ||
                       ush_browser_css_name_eq(decl + name_start, name_end - name_start, "border-top-color") != 0 ||
                       ush_browser_css_name_eq(decl + name_start, name_end - name_start, "border-right-color") != 0 ||
                       ush_browser_css_name_eq(decl + name_start, name_end - name_start, "border-bottom-color") != 0 ||
                       ush_browser_css_name_eq(decl + name_start, name_end - name_start, "border-left-color") != 0 ||
                       ush_browser_css_name_eq(decl + name_start, name_end - name_start, "outline-color") != 0) {
                if (ush_browser_css_parse_first_color_token(value, value_len, &rgb) != 0) {
                    io_delta->set_fg = 1;
                    io_delta->fg_rgb = rgb;
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
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "font-style") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "italic") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "oblique") != 0) {
                    io_delta->set_italic = 1;
                    io_delta->italic = 1;
                } else if (ush_browser_line_has_token_icase(value, value_len, "normal") != 0) {
                    io_delta->set_italic = 1;
                    io_delta->italic = 0;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "font-size") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "small") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "0.") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "8px") != 0 ||
                    ush_browser_line_has_token_icase(value, value_len, "9px") != 0) {
                    io_delta->set_dim = 1;
                    io_delta->dim = 1;
                } else if (ush_browser_line_has_token_icase(value, value_len, "large") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "x-large") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "xx-large") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "2em") != 0 ||
                           ush_browser_line_has_token_icase(value, value_len, "32px") != 0) {
                    io_delta->set_bold = 1;
                    io_delta->bold = 1;
                    io_delta->set_font_scale = 1;
                    io_delta->font_scale =
                        (ush_browser_line_has_token_icase(value, value_len, "xx-large") != 0 ||
                         ush_browser_line_has_token_icase(value, value_len, "32px") != 0)
                            ? 3
                            : 2;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "opacity") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "0") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 1;
                } else if (ush_browser_line_has_token_icase(value, value_len, ".") != 0) {
                    io_delta->set_dim = 1;
                    io_delta->dim = 1;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "text-transform") != 0) {
                io_delta->set_text_transform = 1;
                if (ush_browser_line_has_token_icase(value, value_len, "uppercase") != 0) {
                    io_delta->text_transform = USH_BROWSER_TEXT_TRANSFORM_UPPERCASE;
                } else if (ush_browser_line_has_token_icase(value, value_len, "lowercase") != 0) {
                    io_delta->text_transform = USH_BROWSER_TEXT_TRANSFORM_LOWERCASE;
                } else if (ush_browser_line_has_token_icase(value, value_len, "capitalize") != 0) {
                    io_delta->text_transform = USH_BROWSER_TEXT_TRANSFORM_CAPITALIZE;
                } else {
                    io_delta->text_transform = USH_BROWSER_TEXT_TRANSFORM_NONE;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "white-space") != 0) {
                io_delta->set_white_space_pre = 1;
                io_delta->white_space_pre =
                    (ush_browser_line_has_token_icase(value, value_len, "pre") != 0) ? 1 : 0;
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "display") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "none") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 1;
                } else {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 0;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "visibility") != 0) {
                if (ush_browser_line_has_token_icase(value, value_len, "hidden") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 1;
                } else if (ush_browser_line_has_token_icase(value, value_len, "visible") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 0;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "content") != 0) {
                if (ush_browser_css_value_is_keyword(value, value_len, "none") != 0) {
                    io_delta->set_display_none = 1;
                    io_delta->display_none = 1;
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
        if (ch == ':') {
            break;
        }
        if (ch == '[') {
            while (pos < selector_len && selector[pos] != ']') {
                pos++;
            }
            if (pos < selector_len && selector[pos] == ']') {
                pos++;
            }
            matched = 1;
            continue;
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

void ush_browser_css_scan_style_nodes(GumboNode *node) {
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

void ush_browser_css_apply_rules_for_node(GumboNode *node, ush_browser_style *io_style) {
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
