#include <uwm_uilib.h>

#define UWM_UI_GLYPH7(r0, r1, r2, r3, r4, r5, r6)                                                                      \
    (((u64)(r0) << 30U) | ((u64)(r1) << 25U) | ((u64)(r2) << 20U) | ((u64)(r3) << 15U) | ((u64)(r4) << 10U) |          \
     ((u64)(r5) << 5U) | (u64)(r6))

uwm_ui_surface uwm_uilib_surface(uwm_ui_color *pixels, int width, int height, int pitch_pixels) {
    uwm_ui_surface surface;

    surface.pixels = pixels;
    surface.width = width;
    surface.height = height;
    surface.pitch_pixels = (pitch_pixels > 0) ? pitch_pixels : width;
    return surface;
}

int uwm_uilib_clampi(int value, int min_value, int max_value) {
    if (max_value < min_value) {
        return min_value;
    }
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

char uwm_uilib_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

u64 uwm_uilib_glyph_mask(char ch) {
    switch (uwm_uilib_upper_char(ch)) {
    case 'A':
        return UWM_UI_GLYPH7(14U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'B':
        return UWM_UI_GLYPH7(30U, 17U, 17U, 30U, 17U, 17U, 30U);
    case 'C':
        return UWM_UI_GLYPH7(14U, 17U, 16U, 16U, 16U, 17U, 14U);
    case 'D':
        return UWM_UI_GLYPH7(30U, 17U, 17U, 17U, 17U, 17U, 30U);
    case 'E':
        return UWM_UI_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 31U);
    case 'F':
        return UWM_UI_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 16U);
    case 'G':
        return UWM_UI_GLYPH7(14U, 17U, 16U, 23U, 17U, 17U, 15U);
    case 'H':
        return UWM_UI_GLYPH7(17U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'I':
        return UWM_UI_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 31U);
    case 'J':
        return UWM_UI_GLYPH7(1U, 1U, 1U, 1U, 17U, 17U, 14U);
    case 'K':
        return UWM_UI_GLYPH7(17U, 18U, 20U, 24U, 20U, 18U, 17U);
    case 'L':
        return UWM_UI_GLYPH7(16U, 16U, 16U, 16U, 16U, 16U, 31U);
    case 'M':
        return UWM_UI_GLYPH7(17U, 27U, 21U, 21U, 17U, 17U, 17U);
    case 'N':
        return UWM_UI_GLYPH7(17U, 25U, 21U, 19U, 17U, 17U, 17U);
    case 'O':
        return UWM_UI_GLYPH7(14U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'P':
        return UWM_UI_GLYPH7(30U, 17U, 17U, 30U, 16U, 16U, 16U);
    case 'Q':
        return UWM_UI_GLYPH7(14U, 17U, 17U, 17U, 21U, 18U, 13U);
    case 'R':
        return UWM_UI_GLYPH7(30U, 17U, 17U, 30U, 20U, 18U, 17U);
    case 'S':
        return UWM_UI_GLYPH7(15U, 16U, 16U, 14U, 1U, 1U, 30U);
    case 'T':
        return UWM_UI_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 4U);
    case 'U':
        return UWM_UI_GLYPH7(17U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'V':
        return UWM_UI_GLYPH7(17U, 17U, 17U, 17U, 17U, 10U, 4U);
    case 'W':
        return UWM_UI_GLYPH7(17U, 17U, 17U, 21U, 21U, 21U, 10U);
    case 'X':
        return UWM_UI_GLYPH7(17U, 17U, 10U, 4U, 10U, 17U, 17U);
    case 'Y':
        return UWM_UI_GLYPH7(17U, 17U, 10U, 4U, 4U, 4U, 4U);
    case 'Z':
        return UWM_UI_GLYPH7(31U, 1U, 2U, 4U, 8U, 16U, 31U);
    case '0':
        return UWM_UI_GLYPH7(14U, 17U, 19U, 21U, 25U, 17U, 14U);
    case '1':
        return UWM_UI_GLYPH7(4U, 12U, 4U, 4U, 4U, 4U, 14U);
    case '2':
        return UWM_UI_GLYPH7(14U, 17U, 1U, 2U, 4U, 8U, 31U);
    case '3':
        return UWM_UI_GLYPH7(30U, 1U, 1U, 14U, 1U, 1U, 30U);
    case '4':
        return UWM_UI_GLYPH7(2U, 6U, 10U, 18U, 31U, 2U, 2U);
    case '5':
        return UWM_UI_GLYPH7(31U, 16U, 16U, 30U, 1U, 1U, 30U);
    case '6':
        return UWM_UI_GLYPH7(14U, 16U, 16U, 30U, 17U, 17U, 14U);
    case '7':
        return UWM_UI_GLYPH7(31U, 1U, 2U, 4U, 8U, 8U, 8U);
    case '8':
        return UWM_UI_GLYPH7(14U, 17U, 17U, 14U, 17U, 17U, 14U);
    case '9':
        return UWM_UI_GLYPH7(14U, 17U, 17U, 15U, 1U, 1U, 14U);
    case '-':
        return UWM_UI_GLYPH7(0U, 0U, 0U, 31U, 0U, 0U, 0U);
    case '_':
        return UWM_UI_GLYPH7(0U, 0U, 0U, 0U, 0U, 0U, 31U);
    case '.':
        return UWM_UI_GLYPH7(0U, 0U, 0U, 0U, 0U, 12U, 12U);
    case ':':
        return UWM_UI_GLYPH7(0U, 12U, 12U, 0U, 12U, 12U, 0U);
    case '/':
        return UWM_UI_GLYPH7(1U, 1U, 2U, 4U, 8U, 16U, 16U);
    case '\\':
        return UWM_UI_GLYPH7(16U, 16U, 8U, 4U, 2U, 1U, 1U);
    case '+':
        return UWM_UI_GLYPH7(0U, 4U, 4U, 31U, 4U, 4U, 0U);
    case '=':
        return UWM_UI_GLYPH7(0U, 0U, 31U, 0U, 31U, 0U, 0U);
    case '<':
        return UWM_UI_GLYPH7(1U, 2U, 4U, 8U, 4U, 2U, 1U);
    case '>':
        return UWM_UI_GLYPH7(16U, 8U, 4U, 2U, 4U, 8U, 16U);
    case '[':
        return UWM_UI_GLYPH7(14U, 8U, 8U, 8U, 8U, 8U, 14U);
    case ']':
        return UWM_UI_GLYPH7(14U, 2U, 2U, 2U, 2U, 2U, 14U);
    case '(':
        return UWM_UI_GLYPH7(2U, 4U, 8U, 8U, 8U, 4U, 2U);
    case ')':
        return UWM_UI_GLYPH7(8U, 4U, 2U, 2U, 2U, 4U, 8U);
    case '|':
        return UWM_UI_GLYPH7(4U, 4U, 4U, 4U, 4U, 4U, 4U);
    case '!':
        return UWM_UI_GLYPH7(4U, 4U, 4U, 4U, 4U, 0U, 4U);
    case '?':
        return UWM_UI_GLYPH7(14U, 17U, 1U, 2U, 4U, 0U, 4U);
    case '*':
        return UWM_UI_GLYPH7(0U, 21U, 14U, 31U, 14U, 21U, 0U);
    case '^':
        return UWM_UI_GLYPH7(4U, 10U, 17U, 0U, 0U, 0U, 0U);
    case ',':
        return UWM_UI_GLYPH7(0U, 0U, 0U, 0U, 0U, 4U, 8U);
    case ';':
        return UWM_UI_GLYPH7(0U, 4U, 4U, 0U, 0U, 4U, 8U);
    case '#':
        return UWM_UI_GLYPH7(10U, 31U, 10U, 10U, 31U, 10U, 10U);
    case '$':
        return UWM_UI_GLYPH7(4U, 15U, 20U, 14U, 5U, 30U, 4U);
    case '%':
        return UWM_UI_GLYPH7(24U, 25U, 2U, 4U, 8U, 19U, 3U);
    case '&':
        return UWM_UI_GLYPH7(12U, 18U, 20U, 8U, 21U, 18U, 13U);
    case '@':
        return UWM_UI_GLYPH7(14U, 17U, 23U, 21U, 23U, 16U, 15U);
    case '~':
        return UWM_UI_GLYPH7(0U, 0U, 8U, 21U, 2U, 0U, 0U);
    case '"':
        return UWM_UI_GLYPH7(10U, 10U, 10U, 0U, 0U, 0U, 0U);
    case '\'':
        return UWM_UI_GLYPH7(4U, 4U, 8U, 0U, 0U, 0U, 0U);
    default:
        return 0ULL;
    }
}

void uwm_uilib_fill_rect(const uwm_ui_surface *surface, int x, int y, int w, int h, uwm_ui_color color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (surface == (const uwm_ui_surface *)0 || surface->pixels == (uwm_ui_color *)0 || surface->width <= 0 ||
        surface->height <= 0 || surface->pitch_pixels < surface->width || w <= 0 || h <= 0) {
        return;
    }

    left = uwm_uilib_clampi(x, 0, surface->width);
    top = uwm_uilib_clampi(y, 0, surface->height);
    right = uwm_uilib_clampi(x + w, 0, surface->width);
    bottom = uwm_uilib_clampi(y + h, 0, surface->height);
    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        uwm_ui_color *dst = surface->pixels + ((u64)(unsigned int)row * (u64)(unsigned int)surface->pitch_pixels);
        int col;
        for (col = left; col < right; col++) {
            dst[col] = color;
        }
    }
}

void uwm_uilib_stroke_rect(const uwm_ui_surface *surface, int x, int y, int w, int h, uwm_ui_color color) {
    uwm_uilib_fill_rect(surface, x, y, w, 1, color);
    uwm_uilib_fill_rect(surface, x, y + h - 1, w, 1, color);
    uwm_uilib_fill_rect(surface, x, y, 1, h, color);
    uwm_uilib_fill_rect(surface, x + w - 1, y, 1, h, color);
}

void uwm_uilib_draw_char(const uwm_ui_surface *surface, int x, int y, char ch, int scale, uwm_ui_color color) {
    u64 mask = uwm_uilib_glyph_mask(ch);
    int row;

    if (mask == 0ULL || scale <= 0) {
        return;
    }
    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit)) != 0ULL) {
                uwm_uilib_fill_rect(surface, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

void uwm_uilib_draw_text_limit(const uwm_ui_surface *surface, int x, int y, const char *text, int scale,
                               uwm_ui_color color, int max_x) {
    int cursor_x = x;

    if (surface == (const uwm_ui_surface *)0 || text == (const char *)0 || scale <= 0) {
        return;
    }
    if (max_x <= 0 || max_x > surface->width) {
        max_x = surface->width;
    }
    while (*text != '\0' && cursor_x + (5 * scale) <= max_x) {
        if (*text != ' ') {
            uwm_uilib_draw_char(surface, cursor_x, y, *text, scale, color);
        }
        cursor_x += 6 * scale;
        text++;
    }
}

void uwm_uilib_draw_text(const uwm_ui_surface *surface, int x, int y, const char *text, int scale, uwm_ui_color color) {
    uwm_uilib_draw_text_limit(surface, x, y, text, scale, color,
                              surface != (const uwm_ui_surface *)0 ? surface->width : 0);
}

void uwm_uilib_draw_button(const uwm_ui_surface *surface, int x, int y, int w, int h, const char *label,
                           uwm_ui_color bg, uwm_ui_color hot_bg, uwm_ui_color text, uwm_ui_color border, int hot) {
    uwm_uilib_fill_rect(surface, x, y, w, h, hot != 0 ? hot_bg : bg);
    uwm_uilib_stroke_rect(surface, x, y, w, h, border);
    uwm_uilib_draw_text_limit(surface, x + 10, y + ((h - 7) / 2), label, 1, text, x + w - 8);
}

void uwm_uilib_draw_control_button(const uwm_ui_surface *surface, int x, int y, int w, int h, int active, int kind,
                                   uwm_ui_color text_color) {
    uwm_ui_color bg = (kind == UWM_UI_CONTROL_CLOSE)
                          ? UWM_UI_COLOR_CLOSE
                          : (active != 0 ? UWM_UI_COLOR_CONTROL_ACTIVE : UWM_UI_COLOR_CONTROL_INACTIVE);
    uwm_ui_color fg = (kind == UWM_UI_CONTROL_CLOSE || active != 0) ? UWM_UI_COLOR_WHITE : text_color;
    int cy = y + (h / 2);
    int cx = x + (w / 2);

    uwm_uilib_fill_rect(surface, x, y, w, h, bg);
    if (kind == UWM_UI_CONTROL_MINIMIZE) {
        uwm_uilib_fill_rect(surface, cx - 6, cy + 4, 12, 1, fg);
    } else if (kind == UWM_UI_CONTROL_MAXIMIZE) {
        uwm_uilib_stroke_rect(surface, cx - 6, cy - 6, 12, 12, fg);
        uwm_uilib_fill_rect(surface, cx - 6, cy - 6, 12, 2, fg);
    } else if (kind == UWM_UI_CONTROL_RESTORE) {
        uwm_uilib_stroke_rect(surface, cx - 4, cy - 7, 10, 10, fg);
        uwm_uilib_stroke_rect(surface, cx - 7, cy - 3, 10, 10, fg);
    } else {
        int i;
        for (i = 0; i < 11; i++) {
            uwm_uilib_fill_rect(surface, cx - 5 + i, cy - 5 + i, 1, 1, fg);
            uwm_uilib_fill_rect(surface, cx + 5 - i, cy - 5 + i, 1, 1, fg);
        }
    }
}

u64 uwm_uilib_present(u64 window_id, const uwm_ui_surface *surface) {
    cleonos_wm_present_req req;

    if (window_id == 0ULL || surface == (const uwm_ui_surface *)0 || surface->pixels == (uwm_ui_color *)0 ||
        surface->width <= 0 || surface->height <= 0 || surface->pitch_pixels < surface->width) {
        return 0ULL;
    }

    req.window_id = window_id;
    req.pixels_ptr = (u64)(usize)surface->pixels;
    req.src_width = (u64)(unsigned int)surface->width;
    req.src_height = (u64)(unsigned int)surface->height;
    req.src_pitch_bytes = (u64)(unsigned int)surface->pitch_pixels * 4ULL;
    return cleonos_sys_wm_present(&req);
}
