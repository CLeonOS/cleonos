#include <cleonos_syscall.h>

#include <stdlib.h>
#include <string.h>

typedef long long i64;
typedef unsigned int fx_u32;

#define FX_TTY_DISPLAY 1ULL
#define FX_PATH_MAX 192U
#define FX_NAME_MAX 96U
#define FX_ENTRY_MAX 96U
#define FX_PREVIEW_MAX 768U
#define FX_CANVAS_MAX_PIXELS (900ULL * 640ULL)

#define FX_TITLE_H 32
#define FX_TOOLBAR_H 44
#define FX_STATUS_H 24
#define FX_SIDEBAR_W 136
#define FX_ROW_H 24
#define FX_HEADER_H 24
#define FX_PREVIEW_H 104
#define FX_CLOSE_W 44
#define FX_MIN_W 420
#define FX_MIN_H 360
#define FX_DEFAULT_W 760
#define FX_DEFAULT_H 520

#define FX_KEY_LEFT 1ULL
#define FX_KEY_RIGHT 2ULL
#define FX_KEY_UP 3ULL
#define FX_KEY_DOWN 4ULL

#define FX_COLOR_WHITE 0x00FFFFFFU
#define FX_COLOR_WIN_BLUE 0x000078D7U
#define FX_COLOR_CLOSE 0x00E81123U
#define FX_COLOR_TITLE_INACTIVE 0x00F3F3F3U
#define FX_COLOR_BG 0x00F3F3F3U
#define FX_COLOR_PANEL 0x00FFFFFFU
#define FX_COLOR_SIDEBAR 0x00F7F7F7U
#define FX_COLOR_TEXT 0x00232323U
#define FX_COLOR_MUTED 0x00666666U
#define FX_COLOR_BORDER 0x00D0D0D0U
#define FX_COLOR_SELECT 0x00CDE8FFU
#define FX_COLOR_BUTTON 0x00E7E7E7U
#define FX_COLOR_BUTTON_HOT 0x00D8EBFAU

#define FX_GLYPH7(r0, r1, r2, r3, r4, r5, r6)                                                                          \
    (((u64)(r0) << 30U) | ((u64)(r1) << 25U) | ((u64)(r2) << 20U) | ((u64)(r3) << 15U) | ((u64)(r4) << 10U) |          \
     ((u64)(r5) << 5U) | (u64)(r6))

typedef struct fx_entry {
    char name[FX_NAME_MAX];
    char path[FX_PATH_MAX];
    u64 type;
    u64 size;
} fx_entry;

typedef struct fx_app {
    int screen_w;
    int screen_h;
    int x;
    int y;
    int w;
    int h;
    u64 window_id;
    fx_u32 *pixels;
    u64 pixel_count;
    int running;
    int dragging;
    int drag_dx;
    int drag_dy;
    u64 old_tty;
    int tty_switched;
    char cwd[FX_PATH_MAX];
    fx_entry entries[FX_ENTRY_MAX];
    u64 entry_count;
    int selected;
    int scroll;
    int preview_open;
    char preview[FX_PREVIEW_MAX];
    char status[160];
    int last_click_index;
    u64 last_click_tick;
} fx_app;

static int fx_clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int fx_u64_as_i32(u64 raw) {
    return (int)(i64)raw;
}

static u64 fx_strlen(const char *text) {
    u64 len = 0ULL;

    if (text == (const char *)0) {
        return 0ULL;
    }
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static void fx_copy(char *dst, u64 dst_size, const char *src) {
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL) {
        return;
    }
    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1ULL < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void fx_append(char *dst, u64 dst_size, const char *src) {
    u64 pos;
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL || src == (const char *)0) {
        return;
    }
    pos = fx_strlen(dst);
    while (src[i] != '\0' && pos + 1ULL < dst_size) {
        dst[pos++] = src[i++];
    }
    dst[pos] = '\0';
}

static void fx_append_u64_dec(char *dst, u64 dst_size, u64 value) {
    char tmp[24];
    u64 len = 0ULL;

    if (value == 0ULL) {
        fx_append(dst, dst_size, "0");
        return;
    }
    while (value != 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }
    while (len > 0ULL) {
        char one[2];
        one[0] = tmp[--len];
        one[1] = '\0';
        fx_append(dst, dst_size, one);
    }
}

static void fx_set_status(fx_app *app, const char *text) {
    if (app != (fx_app *)0) {
        fx_copy(app->status, (u64)sizeof(app->status), text);
    }
}

static int fx_streq(const char *a, const char *b) {
    u64 i = 0ULL;

    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return (a[i] == '\0' && b[i] == '\0') ? 1 : 0;
}

static char fx_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

static int fx_char_equal_icase(char a, char b) {
    return (fx_upper_char(a) == fx_upper_char(b)) ? 1 : 0;
}

static int fx_has_suffix_icase(const char *text, const char *suffix) {
    u64 text_len = fx_strlen(text);
    u64 suffix_len = fx_strlen(suffix);
    u64 i;

    if (text_len < suffix_len) {
        return 0;
    }
    for (i = 0ULL; i < suffix_len; i++) {
        if (fx_char_equal_icase(text[text_len - suffix_len + i], suffix[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static int fx_name_compare(const char *a, const char *b) {
    u64 i = 0ULL;

    while (a[i] != '\0' && b[i] != '\0') {
        char ac = fx_upper_char(a[i]);
        char bc = fx_upper_char(b[i]);
        if (ac < bc) {
            return -1;
        }
        if (ac > bc) {
            return 1;
        }
        i++;
    }
    if (a[i] == '\0' && b[i] == '\0') {
        return 0;
    }
    return (a[i] == '\0') ? -1 : 1;
}

static int fx_join_path(const char *dir, const char *name, char *out, u64 out_size) {
    u64 p = 0ULL;
    u64 i;

    if (dir == (const char *)0 || name == (const char *)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }
    if (dir[0] == '/' && dir[1] == '\0') {
        if (p + 1ULL >= out_size) {
            return 0;
        }
        out[p++] = '/';
    } else {
        for (i = 0ULL; dir[i] != '\0'; i++) {
            if (p + 1ULL >= out_size) {
                return 0;
            }
            out[p++] = dir[i];
        }
        if (p > 0ULL && out[p - 1ULL] != '/') {
            if (p + 1ULL >= out_size) {
                return 0;
            }
            out[p++] = '/';
        }
    }
    for (i = 0ULL; name[i] != '\0'; i++) {
        if (p + 1ULL >= out_size) {
            return 0;
        }
        out[p++] = name[i];
    }
    out[p] = '\0';
    return 1;
}

static void fx_parent_path(const char *path, char *out, u64 out_size) {
    u64 len;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    fx_copy(out, out_size, path);
    len = fx_strlen(out);
    if (len <= 1ULL) {
        fx_copy(out, out_size, "/");
        return;
    }
    while (len > 1ULL && out[len - 1ULL] == '/') {
        out[--len] = '\0';
    }
    while (len > 1ULL && out[len - 1ULL] != '/') {
        out[--len] = '\0';
    }
    if (len > 1ULL && out[len - 1ULL] == '/') {
        out[len - 1ULL] = '\0';
    }
    if (out[0] == '\0') {
        fx_copy(out, out_size, "/");
    }
}

static int fx_valid_dir(const char *path) {
    return (cleonos_sys_fs_stat_type(path) == 2ULL) ? 1 : 0;
}

static void fx_sort_entries(fx_app *app) {
    u64 i;

    if (app == (fx_app *)0) {
        return;
    }
    for (i = 1ULL; i < app->entry_count; i++) {
        fx_entry key = app->entries[i];
        u64 j = i;

        while (j > 0ULL) {
            fx_entry *prev = &app->entries[j - 1ULL];
            int move = 0;

            if (prev->type != 2ULL && key.type == 2ULL) {
                move = 1;
            } else if (prev->type == key.type && fx_name_compare(prev->name, key.name) > 0) {
                move = 1;
            }
            if (move == 0) {
                break;
            }
            app->entries[j] = app->entries[j - 1ULL];
            j--;
        }
        app->entries[j] = key;
    }
}

static int fx_load_dir(fx_app *app, const char *path) {
    u64 count;
    u64 i;

    if (app == (fx_app *)0 || path == (const char *)0 || fx_valid_dir(path) == 0) {
        return 0;
    }

    fx_copy(app->cwd, (u64)sizeof(app->cwd), path);
    app->entry_count = 0ULL;
    app->selected = 0;
    app->scroll = 0;
    app->preview_open = 0;
    app->preview[0] = '\0';

    count = cleonos_sys_fs_child_count(path);
    if (count > FX_ENTRY_MAX) {
        count = FX_ENTRY_MAX;
    }

    for (i = 0ULL; i < count; i++) {
        fx_entry *entry = &app->entries[app->entry_count];

        entry->name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(path, i, entry->name) == 0ULL) {
            continue;
        }
        if (entry->name[0] == '\0') {
            continue;
        }
        if (fx_join_path(path, entry->name, entry->path, (u64)sizeof(entry->path)) == 0) {
            continue;
        }
        entry->type = cleonos_sys_fs_stat_type(entry->path);
        entry->size = (entry->type == 1ULL) ? cleonos_sys_fs_stat_size(entry->path) : 0ULL;
        app->entry_count++;
    }

    fx_sort_entries(app);
    if (app->entry_count == 0ULL) {
        app->selected = -1;
        fx_set_status(app, "Folder is empty");
    } else {
        fx_set_status(app, "Ready");
    }
    return 1;
}

static void fx_format_size(char *out, u64 out_size, u64 size) {
    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    out[0] = '\0';
    if (size >= 1048576ULL) {
        fx_append_u64_dec(out, out_size, size / 1048576ULL);
        fx_append(out, out_size, " MB");
    } else if (size >= 1024ULL) {
        fx_append_u64_dec(out, out_size, size / 1024ULL);
        fx_append(out, out_size, " KB");
    } else {
        fx_append_u64_dec(out, out_size, size);
        fx_append(out, out_size, " B");
    }
}

static void fx_sanitize_preview(char *buffer, u64 *io_len) {
    u64 r;
    u64 w = 0ULL;
    u64 len;

    if (buffer == (char *)0 || io_len == (u64 *)0) {
        return;
    }
    len = *io_len;
    for (r = 0ULL; r < len && w + 1ULL < (u64)FX_PREVIEW_MAX; r++) {
        unsigned char ch = (unsigned char)buffer[r];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || ch == '\t' || (ch >= 32U && ch <= 126U)) {
            buffer[w++] = (char)((ch == '\t') ? ' ' : ch);
        } else {
            buffer[w++] = '.';
        }
    }
    buffer[w] = '\0';
    *io_len = w;
}

static void fx_open_selected(fx_app *app) {
    fx_entry *entry;

    if (app == (fx_app *)0 || app->selected < 0 || (u64)app->selected >= app->entry_count) {
        return;
    }
    entry = &app->entries[(u64)app->selected];
    if (entry->type == 2ULL) {
        if (fx_load_dir(app, entry->path) == 0) {
            fx_set_status(app, "Open folder failed");
        }
        return;
    }
    if (entry->type != 1ULL) {
        fx_set_status(app, "Unknown entry type");
        return;
    }
    if (fx_has_suffix_icase(entry->name, ".elf") != 0) {
        u64 pid = cleonos_sys_spawn_pathv(entry->path, "", "LAUNCHED_BY=file_explorer");
        app->preview_open = 0;
        if (pid == (u64)-1 || pid == 0ULL) {
            fx_set_status(app, "Launch failed");
        } else {
            fx_copy(app->status, (u64)sizeof(app->status), "Launched ");
            fx_append(app->status, (u64)sizeof(app->status), entry->name);
        }
        return;
    }

    {
        u64 got = cleonos_sys_fs_read(entry->path, app->preview, (u64)sizeof(app->preview) - 1ULL);
        if (got == (u64)-1) {
            app->preview_open = 0;
            fx_set_status(app, "Read failed");
            return;
        }
        fx_sanitize_preview(app->preview, &got);
        app->preview_open = 1;
        fx_copy(app->status, (u64)sizeof(app->status), "Preview ");
        fx_append(app->status, (u64)sizeof(app->status), entry->name);
    }
}

static void fx_go_parent(fx_app *app) {
    char parent[FX_PATH_MAX];

    if (app == (fx_app *)0) {
        return;
    }
    fx_parent_path(app->cwd, parent, (u64)sizeof(parent));
    if (fx_load_dir(app, parent) == 0) {
        fx_set_status(app, "Parent folder unavailable");
    }
}

static u64 fx_glyph_mask(char ch) {
    switch (fx_upper_char(ch)) {
    case 'A':
        return FX_GLYPH7(14U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'B':
        return FX_GLYPH7(30U, 17U, 17U, 30U, 17U, 17U, 30U);
    case 'C':
        return FX_GLYPH7(14U, 17U, 16U, 16U, 16U, 17U, 14U);
    case 'D':
        return FX_GLYPH7(30U, 17U, 17U, 17U, 17U, 17U, 30U);
    case 'E':
        return FX_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 31U);
    case 'F':
        return FX_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 16U);
    case 'G':
        return FX_GLYPH7(14U, 17U, 16U, 23U, 17U, 17U, 15U);
    case 'H':
        return FX_GLYPH7(17U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'I':
        return FX_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 31U);
    case 'J':
        return FX_GLYPH7(1U, 1U, 1U, 1U, 17U, 17U, 14U);
    case 'K':
        return FX_GLYPH7(17U, 18U, 20U, 24U, 20U, 18U, 17U);
    case 'L':
        return FX_GLYPH7(16U, 16U, 16U, 16U, 16U, 16U, 31U);
    case 'M':
        return FX_GLYPH7(17U, 27U, 21U, 21U, 17U, 17U, 17U);
    case 'N':
        return FX_GLYPH7(17U, 25U, 21U, 19U, 17U, 17U, 17U);
    case 'O':
        return FX_GLYPH7(14U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'P':
        return FX_GLYPH7(30U, 17U, 17U, 30U, 16U, 16U, 16U);
    case 'Q':
        return FX_GLYPH7(14U, 17U, 17U, 17U, 21U, 18U, 13U);
    case 'R':
        return FX_GLYPH7(30U, 17U, 17U, 30U, 20U, 18U, 17U);
    case 'S':
        return FX_GLYPH7(15U, 16U, 16U, 14U, 1U, 1U, 30U);
    case 'T':
        return FX_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 4U);
    case 'U':
        return FX_GLYPH7(17U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'V':
        return FX_GLYPH7(17U, 17U, 17U, 17U, 17U, 10U, 4U);
    case 'W':
        return FX_GLYPH7(17U, 17U, 17U, 21U, 21U, 21U, 10U);
    case 'X':
        return FX_GLYPH7(17U, 17U, 10U, 4U, 10U, 17U, 17U);
    case 'Y':
        return FX_GLYPH7(17U, 17U, 10U, 4U, 4U, 4U, 4U);
    case 'Z':
        return FX_GLYPH7(31U, 1U, 2U, 4U, 8U, 16U, 31U);
    case '0':
        return FX_GLYPH7(14U, 17U, 19U, 21U, 25U, 17U, 14U);
    case '1':
        return FX_GLYPH7(4U, 12U, 4U, 4U, 4U, 4U, 14U);
    case '2':
        return FX_GLYPH7(14U, 17U, 1U, 2U, 4U, 8U, 31U);
    case '3':
        return FX_GLYPH7(30U, 1U, 1U, 14U, 1U, 1U, 30U);
    case '4':
        return FX_GLYPH7(2U, 6U, 10U, 18U, 31U, 2U, 2U);
    case '5':
        return FX_GLYPH7(31U, 16U, 16U, 30U, 1U, 1U, 30U);
    case '6':
        return FX_GLYPH7(14U, 16U, 16U, 30U, 17U, 17U, 14U);
    case '7':
        return FX_GLYPH7(31U, 1U, 2U, 4U, 8U, 8U, 8U);
    case '8':
        return FX_GLYPH7(14U, 17U, 17U, 14U, 17U, 17U, 14U);
    case '9':
        return FX_GLYPH7(14U, 17U, 17U, 15U, 1U, 1U, 14U);
    case '-':
        return FX_GLYPH7(0U, 0U, 0U, 31U, 0U, 0U, 0U);
    case '_':
        return FX_GLYPH7(0U, 0U, 0U, 0U, 0U, 0U, 31U);
    case '.':
        return FX_GLYPH7(0U, 0U, 0U, 0U, 0U, 12U, 12U);
    case ':':
        return FX_GLYPH7(0U, 12U, 12U, 0U, 12U, 12U, 0U);
    case '/':
        return FX_GLYPH7(1U, 1U, 2U, 4U, 8U, 16U, 16U);
    case '+':
        return FX_GLYPH7(0U, 4U, 4U, 31U, 4U, 4U, 0U);
    default:
        return 0ULL;
    }
}

static void fx_fill_rect(fx_app *app, int x, int y, int w, int h, fx_u32 color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (app == (fx_app *)0 || app->pixels == (fx_u32 *)0 || w <= 0 || h <= 0) {
        return;
    }
    left = fx_clampi(x, 0, app->w);
    top = fx_clampi(y, 0, app->h);
    right = fx_clampi(x + w, 0, app->w);
    bottom = fx_clampi(y + h, 0, app->h);
    if (left >= right || top >= bottom) {
        return;
    }
    for (row = top; row < bottom; row++) {
        u64 base = (u64)(unsigned int)row * (u64)(unsigned int)app->w;
        int col;
        for (col = left; col < right; col++) {
            app->pixels[base + (u64)(unsigned int)col] = color;
        }
    }
}

static void fx_stroke_rect(fx_app *app, int x, int y, int w, int h, fx_u32 color) {
    fx_fill_rect(app, x, y, w, 1, color);
    fx_fill_rect(app, x, y + h - 1, w, 1, color);
    fx_fill_rect(app, x, y, 1, h, color);
    fx_fill_rect(app, x + w - 1, y, 1, h, color);
}

static void fx_draw_char(fx_app *app, int x, int y, char ch, int scale, fx_u32 color) {
    u64 mask = fx_glyph_mask(ch);
    int row;

    if (mask == 0ULL || scale <= 0) {
        return;
    }
    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit)) != 0ULL) {
                fx_fill_rect(app, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

static void fx_draw_text_limit(fx_app *app, int x, int y, const char *text, int scale, fx_u32 color, int max_x) {
    int cursor_x = x;

    if (app == (fx_app *)0 || text == (const char *)0 || scale <= 0) {
        return;
    }
    if (max_x <= 0 || max_x > app->w) {
        max_x = app->w;
    }
    while (*text != '\0' && cursor_x + (5 * scale) <= max_x) {
        if (*text != ' ') {
            fx_draw_char(app, cursor_x, y, *text, scale, color);
        }
        cursor_x += 6 * scale;
        text++;
    }
}

static void fx_draw_text(fx_app *app, int x, int y, const char *text, int scale, fx_u32 color) {
    fx_draw_text_limit(app, x, y, text, scale, color, app != (fx_app *)0 ? app->w : 0);
}

static void fx_draw_button(fx_app *app, int x, int y, int w, int h, const char *label, int hot) {
    fx_fill_rect(app, x, y, w, h, hot != 0 ? FX_COLOR_BUTTON_HOT : FX_COLOR_BUTTON);
    fx_stroke_rect(app, x, y, w, h, FX_COLOR_BORDER);
    fx_draw_text_limit(app, x + 10, y + ((h - 7) / 2), label, 1, FX_COLOR_TEXT, x + w - 8);
}

static void fx_draw_preview(fx_app *app, int y, int h) {
    int line = 0;
    int cursor_y;
    u64 i = 0ULL;
    char tmp[96];
    u64 p = 0ULL;

    fx_fill_rect(app, FX_SIDEBAR_W, y, app->w - FX_SIDEBAR_W, h, 0x00FAFAFAU);
    fx_stroke_rect(app, FX_SIDEBAR_W, y, app->w - FX_SIDEBAR_W, h, FX_COLOR_BORDER);
    fx_draw_text(app, FX_SIDEBAR_W + 12, y + 10, "PREVIEW", 1, FX_COLOR_MUTED);
    cursor_y = y + 28;

    while (app->preview[i] != '\0' && line < 4) {
        char ch = app->preview[i++];
        if (ch == '\n' || p + 1ULL >= (u64)sizeof(tmp)) {
            tmp[p] = '\0';
            fx_draw_text_limit(app, FX_SIDEBAR_W + 12, cursor_y, tmp, 1, FX_COLOR_TEXT, app->w - 12);
            p = 0ULL;
            cursor_y += 16;
            line++;
            continue;
        }
        tmp[p++] = ch;
    }
    if (p > 0ULL && line < 4) {
        tmp[p] = '\0';
        fx_draw_text_limit(app, FX_SIDEBAR_W + 12, cursor_y, tmp, 1, FX_COLOR_TEXT, app->w - 12);
    }
}

static int fx_visible_rows(const fx_app *app) {
    int top = FX_TITLE_H + FX_TOOLBAR_H + FX_HEADER_H;
    int bottom = app->h - FX_STATUS_H - (app->preview_open != 0 ? FX_PREVIEW_H : 0);
    int rows = (bottom - top) / FX_ROW_H;

    return (rows < 1) ? 1 : rows;
}

static void fx_ensure_selected_visible(fx_app *app) {
    int rows;

    if (app == (fx_app *)0 || app->selected < 0) {
        return;
    }
    rows = fx_visible_rows(app);
    if (app->selected < app->scroll) {
        app->scroll = app->selected;
    }
    if (app->selected >= app->scroll + rows) {
        app->scroll = app->selected - rows + 1;
    }
    if (app->scroll < 0) {
        app->scroll = 0;
    }
}

static void fx_render(fx_app *app) {
    int list_top = FX_TITLE_H + FX_TOOLBAR_H;
    int rows_top = list_top + FX_HEADER_H;
    int preview_y = app->h - FX_STATUS_H - (app->preview_open != 0 ? FX_PREVIEW_H : 0);
    int list_bottom = preview_y;
    int rows;
    int i;
    const char *quick_names[] = {"ROOT", "SYSTEM", "SHELL", "UWM", "TEMP", "DRIVER", "DEV"};
    const char *quick_paths[] = {"/", "/system", "/shell", "/shell/uwm", "/temp", "/driver", "/dev"};

    fx_fill_rect(app, 0, 0, app->w, app->h, FX_COLOR_BG);
    fx_fill_rect(app, 0, 0, app->w, FX_TITLE_H, FX_COLOR_WIN_BLUE);
    fx_draw_text(app, 12, 12, "FILE EXPLORER", 1, FX_COLOR_WHITE);
    fx_fill_rect(app, app->w - FX_CLOSE_W, 0, FX_CLOSE_W, FX_TITLE_H, FX_COLOR_CLOSE);
    fx_fill_rect(app, app->w - 28, 14, 14, 2, FX_COLOR_WHITE);
    fx_fill_rect(app, app->w - 22, 9, 2, 12, FX_COLOR_WHITE);

    fx_fill_rect(app, 0, FX_TITLE_H, app->w, FX_TOOLBAR_H, FX_COLOR_PANEL);
    fx_fill_rect(app, 0, FX_TITLE_H + FX_TOOLBAR_H - 1, app->w, 1, FX_COLOR_BORDER);
    fx_draw_button(app, 10, FX_TITLE_H + 8, 52, 28, "UP", 0);
    fx_draw_button(app, 70, FX_TITLE_H + 8, 66, 28, "ROOT", 0);
    fx_draw_button(app, 144, FX_TITLE_H + 8, 66, 28, "TEMP", 0);
    fx_draw_button(app, 218, FX_TITLE_H + 8, 84, 28, "REFRESH", 0);
    fx_fill_rect(app, 314, FX_TITLE_H + 8, app->w - 326, 28, 0x00F8F8F8U);
    fx_stroke_rect(app, 314, FX_TITLE_H + 8, app->w - 326, 28, FX_COLOR_BORDER);
    fx_draw_text_limit(app, 324, FX_TITLE_H + 18, app->cwd, 1, FX_COLOR_TEXT, app->w - 14);

    fx_fill_rect(app, 0, list_top, FX_SIDEBAR_W, app->h - list_top - FX_STATUS_H, FX_COLOR_SIDEBAR);
    fx_fill_rect(app, FX_SIDEBAR_W - 1, list_top, 1, app->h - list_top - FX_STATUS_H, FX_COLOR_BORDER);
    for (i = 0; i < 7; i++) {
        int y = list_top + 16 + (i * 32);
        int active = fx_streq(app->cwd, quick_paths[i]);
        if (active != 0) {
            fx_fill_rect(app, 8, y - 8, FX_SIDEBAR_W - 16, 26, FX_COLOR_SELECT);
        }
        fx_draw_text_limit(app, 18, y, quick_names[i], 1, active != 0 ? FX_COLOR_TEXT : FX_COLOR_MUTED,
                           FX_SIDEBAR_W - 10);
    }

    fx_fill_rect(app, FX_SIDEBAR_W, list_top, app->w - FX_SIDEBAR_W, FX_HEADER_H, 0x00EFEFEFU);
    fx_fill_rect(app, FX_SIDEBAR_W, list_top + FX_HEADER_H - 1, app->w - FX_SIDEBAR_W, 1, FX_COLOR_BORDER);
    fx_draw_text(app, FX_SIDEBAR_W + 12, list_top + 9, "NAME", 1, FX_COLOR_MUTED);
    fx_draw_text(app, app->w - 202, list_top + 9, "TYPE", 1, FX_COLOR_MUTED);
    fx_draw_text(app, app->w - 102, list_top + 9, "SIZE", 1, FX_COLOR_MUTED);

    rows = (list_bottom - rows_top) / FX_ROW_H;
    if (rows < 1) {
        rows = 1;
    }
    for (i = 0; i < rows; i++) {
        int entry_index = app->scroll + i;
        int y = rows_top + (i * FX_ROW_H);
        fx_entry *entry;
        char size_text[32];

        if ((u64)entry_index >= app->entry_count) {
            break;
        }
        entry = &app->entries[(u64)entry_index];
        if (entry_index == app->selected) {
            fx_fill_rect(app, FX_SIDEBAR_W + 2, y, app->w - FX_SIDEBAR_W - 4, FX_ROW_H, FX_COLOR_SELECT);
        } else if ((i & 1) != 0) {
            fx_fill_rect(app, FX_SIDEBAR_W, y, app->w - FX_SIDEBAR_W, FX_ROW_H, 0x00FAFAFAU);
        }
        fx_draw_text_limit(app, FX_SIDEBAR_W + 12, y + 8, entry->name, 1, FX_COLOR_TEXT, app->w - 214);
        fx_draw_text(app, app->w - 202, y + 8, entry->type == 2ULL ? "FOLDER" : "FILE", 1,
                     entry->type == 2ULL ? FX_COLOR_WIN_BLUE : FX_COLOR_MUTED);
        size_text[0] = '\0';
        if (entry->type == 1ULL) {
            fx_format_size(size_text, (u64)sizeof(size_text), entry->size);
            fx_draw_text_limit(app, app->w - 102, y + 8, size_text, 1, FX_COLOR_MUTED, app->w - 8);
        }
    }
    if (app->entry_count == 0ULL) {
        fx_draw_text(app, FX_SIDEBAR_W + 18, rows_top + 24, "EMPTY FOLDER", 1, FX_COLOR_MUTED);
    }

    if (app->preview_open != 0) {
        fx_draw_preview(app, preview_y, FX_PREVIEW_H);
    }

    fx_fill_rect(app, 0, app->h - FX_STATUS_H, app->w, FX_STATUS_H, 0x00EDEDEDU);
    fx_fill_rect(app, 0, app->h - FX_STATUS_H, app->w, 1, FX_COLOR_BORDER);
    fx_draw_text_limit(app, 12, app->h - 16, app->status, 1, FX_COLOR_MUTED, app->w - 12);
}

static int fx_present(fx_app *app) {
    cleonos_wm_present_req req;

    if (app == (fx_app *)0 || app->window_id == 0ULL || app->pixels == (fx_u32 *)0) {
        return 0;
    }
    req.window_id = app->window_id;
    req.pixels_ptr = (u64)(usize)app->pixels;
    req.src_width = (u64)(unsigned int)app->w;
    req.src_height = (u64)(unsigned int)app->h;
    req.src_pitch_bytes = (u64)(unsigned int)app->w * 4ULL;
    return (cleonos_sys_wm_present(&req) != 0ULL) ? 1 : 0;
}

static int fx_render_present(fx_app *app) {
    fx_render(app);
    return fx_present(app);
}

static int fx_hit_button(int x, int y, int bx, int by, int bw, int bh) {
    return (x >= bx && x < bx + bw && y >= by && y < by + bh) ? 1 : 0;
}

static int fx_sidebar_index_at(int local_y) {
    int top = FX_TITLE_H + FX_TOOLBAR_H + 16 - 8;
    int rel = local_y - top;
    int idx;

    if (rel < 0) {
        return -1;
    }
    idx = rel / 32;
    if (idx < 0 || idx >= 7) {
        return -1;
    }
    if ((rel % 32) >= 26) {
        return -1;
    }
    return idx;
}

static int fx_row_at(const fx_app *app, int local_x, int local_y) {
    int rows_top = FX_TITLE_H + FX_TOOLBAR_H + FX_HEADER_H;
    int row;

    if (app == (const fx_app *)0 || local_x < FX_SIDEBAR_W || local_y < rows_top) {
        return -1;
    }
    row = (local_y - rows_top) / FX_ROW_H;
    if (row < 0 || row >= fx_visible_rows(app)) {
        return -1;
    }
    row += app->scroll;
    if (row < 0 || (u64)row >= app->entry_count) {
        return -1;
    }
    return row;
}

static void fx_select_delta(fx_app *app, int delta) {
    if (app == (fx_app *)0 || app->entry_count == 0ULL) {
        return;
    }
    if (app->selected < 0) {
        app->selected = 0;
    } else {
        app->selected = fx_clampi(app->selected + delta, 0, (int)app->entry_count - 1);
    }
    fx_ensure_selected_visible(app);
}

static void fx_navigate_path(fx_app *app, const char *path) {
    if (fx_load_dir(app, path) == 0) {
        fx_set_status(app, "Folder unavailable");
    }
}

static void fx_refresh(fx_app *app) {
    char path[FX_PATH_MAX];

    if (app == (fx_app *)0) {
        return;
    }
    fx_copy(path, (u64)sizeof(path), app->cwd);
    if (fx_load_dir(app, path) == 0) {
        fx_set_status(app, "Refresh failed");
    }
}

static void fx_handle_key(fx_app *app, u64 key) {
    if (key == (u64)'q' || key == (u64)'Q' || key == 27ULL) {
        app->running = 0;
    } else if (key == FX_KEY_UP || key == (u64)'w' || key == (u64)'W') {
        fx_select_delta(app, -1);
    } else if (key == FX_KEY_DOWN || key == (u64)'s' || key == (u64)'S') {
        fx_select_delta(app, 1);
    } else if (key == 13ULL) {
        fx_open_selected(app);
    } else if (key == 8ULL || key == 127ULL || key == (u64)'u' || key == (u64)'U') {
        fx_go_parent(app);
    } else if (key == (u64)'r' || key == (u64)'R') {
        fx_refresh(app);
    } else if (key == (u64)'h' || key == (u64)'H') {
        fx_navigate_path(app, "/");
    }
}

static void fx_handle_mouse_button(fx_app *app, const cleonos_wm_event *event) {
    static const char *quick_paths[] = {"/", "/system", "/shell", "/shell/uwm", "/temp", "/driver", "/dev"};
    u64 buttons = event->arg0;
    u64 changed = event->arg1;
    int local_x = fx_u64_as_i32(event->arg2);
    int local_y = fx_u64_as_i32(event->arg3);
    int left_changed = ((changed & 0x1ULL) != 0ULL) ? 1 : 0;
    int left_down = ((buttons & 0x1ULL) != 0ULL) ? 1 : 0;

    if (left_changed == 0) {
        return;
    }
    if (left_down == 0) {
        app->dragging = 0;
        return;
    }
    if (local_y >= 0 && local_y < FX_TITLE_H) {
        if (local_x >= app->w - FX_CLOSE_W) {
            app->running = 0;
            return;
        }
        app->dragging = 1;
        app->drag_dx = local_x;
        app->drag_dy = local_y;
        return;
    }
    if (fx_hit_button(local_x, local_y, 10, FX_TITLE_H + 8, 52, 28) != 0) {
        fx_go_parent(app);
        return;
    }
    if (fx_hit_button(local_x, local_y, 70, FX_TITLE_H + 8, 66, 28) != 0) {
        fx_navigate_path(app, "/");
        return;
    }
    if (fx_hit_button(local_x, local_y, 144, FX_TITLE_H + 8, 66, 28) != 0) {
        fx_navigate_path(app, "/temp");
        return;
    }
    if (fx_hit_button(local_x, local_y, 218, FX_TITLE_H + 8, 84, 28) != 0) {
        fx_refresh(app);
        return;
    }
    if (local_x < FX_SIDEBAR_W) {
        int idx = fx_sidebar_index_at(local_y);
        if (idx >= 0) {
            fx_navigate_path(app, quick_paths[idx]);
        }
        return;
    }
    {
        int row = fx_row_at(app, local_x, local_y);
        if (row >= 0) {
            u64 now = cleonos_sys_timer_ticks();
            if (row == app->selected && row == app->last_click_index && now - app->last_click_tick < 40ULL) {
                fx_open_selected(app);
            } else {
                app->selected = row;
                fx_ensure_selected_visible(app);
                app->preview_open = 0;
            }
            app->last_click_index = row;
            app->last_click_tick = now;
        }
    }
}

static void fx_handle_mouse_move(fx_app *app, const cleonos_wm_event *event) {
    if (app->dragging != 0) {
        cleonos_wm_move_req req;
        app->x = fx_u64_as_i32(event->arg0) - app->drag_dx;
        app->y = fx_u64_as_i32(event->arg1) - app->drag_dy;
        req.window_id = app->window_id;
        req.x = (u64)(i64)app->x;
        req.y = (u64)(i64)app->y;
        (void)cleonos_sys_wm_move(&req);
    }
}

static void fx_loop(fx_app *app) {
    while (app->running != 0) {
        int dirty = 0;
        int handled = 0;
        u64 budget = 0ULL;

        while (budget < 96ULL) {
            cleonos_wm_event event;
            memset(&event, 0, sizeof(event));
            if (cleonos_sys_wm_poll_event(app->window_id, &event) == 0ULL) {
                break;
            }
            handled = 1;
            dirty = 1;
            if (event.type == CLEONOS_WM_EVENT_KEY) {
                fx_handle_key(app, event.arg0);
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_BUTTON) {
                fx_handle_mouse_button(app, &event);
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_MOVE) {
                fx_handle_mouse_move(app, &event);
            }
            if (app->running == 0) {
                break;
            }
            budget++;
        }

        if (dirty != 0 && app->running != 0) {
            (void)fx_render_present(app);
        }
        if (handled != 0 || app->dragging != 0) {
            (void)cleonos_sys_yield();
        } else {
            (void)cleonos_sys_sleep_ticks(1ULL);
        }
    }
}

static int fx_choose_geometry(fx_app *app) {
    cleonos_fb_info fb;
    int max_w;
    int max_h;

    memset(&fb, 0, sizeof(fb));
    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL ||
        fb.width > 4096ULL || fb.height > 4096ULL) {
        return 0;
    }
    app->screen_w = (int)fb.width;
    app->screen_h = (int)fb.height;
    max_w = app->screen_w - 96;
    max_h = app->screen_h - 120;
    if (max_w < FX_MIN_W) {
        max_w = app->screen_w;
    }
    if (max_h < FX_MIN_H) {
        max_h = app->screen_h;
    }
    app->w = fx_clampi(FX_DEFAULT_W, FX_MIN_W, max_w);
    app->h = fx_clampi(FX_DEFAULT_H, FX_MIN_H, max_h);
    if ((u64)(unsigned int)app->w * (u64)(unsigned int)app->h > FX_CANVAS_MAX_PIXELS) {
        app->w = 760;
        app->h = 520;
    }
    app->x = (app->screen_w > app->w) ? ((app->screen_w - app->w) / 2) : 0;
    app->y = (app->screen_h > app->h) ? ((app->screen_h - app->h) / 2) : 0;
    return 1;
}

static int fx_init_window(fx_app *app) {
    cleonos_wm_create_req req;
    u64 count;

    if (fx_choose_geometry(app) == 0) {
        return 0;
    }
    count = (u64)(unsigned int)app->w * (u64)(unsigned int)app->h;
    app->pixels = (fx_u32 *)malloc((size_t)(count * 4ULL));
    if (app->pixels == (fx_u32 *)0) {
        return 0;
    }
    app->pixel_count = count;
    memset(app->pixels, 0, (size_t)(count * 4ULL));

    app->old_tty = cleonos_sys_tty_active();
    if (app->old_tty != FX_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(FX_TTY_DISPLAY);
        app->tty_switched = 1;
    }

    req.x = (u64)(i64)app->x;
    req.y = (u64)(i64)app->y;
    req.width = (u64)(unsigned int)app->w;
    req.height = (u64)(unsigned int)app->h;
    req.flags = 0ULL;
    app->window_id = cleonos_sys_wm_create(&req);
    if (app->window_id == 0ULL) {
        return 0;
    }
    (void)cleonos_sys_wm_set_focus(app->window_id);
    return 1;
}

static void fx_destroy(fx_app *app) {
    if (app == (fx_app *)0) {
        return;
    }
    if (app->window_id != 0ULL) {
        (void)cleonos_sys_wm_destroy(app->window_id);
        app->window_id = 0ULL;
    }
    if (app->pixels != (fx_u32 *)0) {
        free(app->pixels);
        app->pixels = (fx_u32 *)0;
    }
    if (app->tty_switched != 0) {
        (void)cleonos_sys_tty_switch(app->old_tty);
        app->tty_switched = 0;
    }
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    fx_app app;
    const char *start_path = "/";

    (void)envp;
    memset(&app, 0, sizeof(app));
    app.selected = -1;
    app.last_click_index = -1;
    app.running = 1;

    if (argc > 1 && argv != (char **)0 && argv[1] != (char *)0 && argv[1][0] == '/') {
        start_path = argv[1];
    }
    if (fx_load_dir(&app, start_path) == 0) {
        (void)fx_load_dir(&app, "/");
    }
    if (fx_init_window(&app) == 0) {
        fx_destroy(&app);
        return 1;
    }
    if (fx_render_present(&app) == 0) {
        fx_destroy(&app);
        return 1;
    }
    fx_loop(&app);
    fx_destroy(&app);
    return 0;
}
