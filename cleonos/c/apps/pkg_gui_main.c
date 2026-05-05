#include <cleonos_syscall.h>
#include <uwm_uilib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pkg/pkg_internal.h"

typedef unsigned int pg_u32;

#define PG_TTY_DISPLAY 1ULL
#define PG_MAX_PACKAGES 64U
#define PG_QUERY_MAX 80U
#define PG_STATUS_MAX 192U
#define PG_CANVAS_MAX_PIXELS (900ULL * 640ULL)

#define PG_DEFAULT_W 860
#define PG_DEFAULT_H 560
#define PG_MIN_W 620
#define PG_MIN_H 420
#define PG_TITLE_H 32
#define PG_TOOLBAR_H 56
#define PG_HEADER_H 26
#define PG_STATUS_H 28
#define PG_ROW_H 28
#define PG_CONTROL_W 46
#define PG_EVENT_BUDGET 96ULL

#define PG_KEY_LEFT 1ULL
#define PG_KEY_RIGHT 2ULL
#define PG_KEY_UP 3ULL
#define PG_KEY_DOWN 4ULL

#define PG_COLOR_WHITE 0x00FFFFFFU
#define PG_COLOR_TITLE 0x000078D7U
#define PG_COLOR_TITLE_INACTIVE 0x00F3F3F3U
#define PG_COLOR_BG 0x00F3F3F3U
#define PG_COLOR_PANEL 0x00FFFFFFU
#define PG_COLOR_PANEL_2 0x00F8F8F8U
#define PG_COLOR_TEXT 0x00232323U
#define PG_COLOR_MUTED 0x00666666U
#define PG_COLOR_BORDER 0x00D0D0D0U
#define PG_COLOR_ROW_ALT 0x00FAFAFAU
#define PG_COLOR_SELECT 0x00CDE8FFU
#define PG_COLOR_SELECT_EDGE 0x000078D7U
#define PG_COLOR_INPUT 0x00FFFFFFU
#define PG_COLOR_BUTTON 0x00E7E7E7U
#define PG_COLOR_BUTTON_HOT 0x00D8EBFAU
#define PG_COLOR_INSTALLED 0x00DFF6DDU
#define PG_COLOR_WARN 0x00FFF4CEU

typedef struct pg_app {
    int screen_w;
    int screen_h;
    int x;
    int y;
    int w;
    int h;
    int restore_x;
    int restore_y;
    int restore_w;
    int restore_h;
    int maximized;
    int focused;
    int dragging;
    int drag_dx;
    int drag_dy;
    int running;
    int search_mode;
    int tty_switched;
    u64 old_tty;
    u64 window_id;
    pg_u32 *pixels;
    u64 pixel_count;
    pkg_remote_package packages[PG_MAX_PACKAGES];
    char installed_versions[PG_MAX_PACKAGES][PKG_VERSION_MAX];
    int installed[PG_MAX_PACKAGES];
    u64 package_count;
    int selected;
    int scroll;
    char query[PG_QUERY_MAX];
    char repo[PKG_URL_MAX];
    char status[PG_STATUS_MAX];
} pg_app;

static pg_app pg_state;

static int pg_clampi(int value, int min_value, int max_value) {
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

static int pg_u64_as_i32(u64 raw) {
    return (int)(i64)raw;
}

static void pg_append(char *dst, u64 dst_size, const char *src) {
    u64 pos;
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL || src == (const char *)0) {
        return;
    }
    pos = ush_strlen(dst);
    while (src[i] != '\0' && pos + 1ULL < dst_size) {
        dst[pos++] = src[i++];
    }
    dst[pos] = '\0';
}

static const char *pg_text_or_dash(const char *text) {
    return (text != (const char *)0 && text[0] != '\0') ? text : "-";
}

static void pg_set_status(pg_app *app, const char *text) {
    if (app != (pg_app *)0) {
        ush_copy(app->status, (u64)sizeof(app->status), text != (const char *)0 ? text : "");
    }
}

static void pg_set_status_name(pg_app *app, const char *prefix, const char *name, const char *suffix) {
    if (app == (pg_app *)0) {
        return;
    }
    app->status[0] = '\0';
    pg_append(app->status, (u64)sizeof(app->status), prefix);
    pg_append(app->status, (u64)sizeof(app->status), name);
    pg_append(app->status, (u64)sizeof(app->status), suffix);
}

static uwm_ui_surface pg_surface(const pg_app *app) {
    if (app == (const pg_app *)0) {
        return uwm_uilib_surface((uwm_ui_color *)0, 0, 0, 0);
    }
    return uwm_uilib_surface(app->pixels, app->w, app->h, app->w);
}

static void pg_fill_rect(pg_app *app, int x, int y, int w, int h, pg_u32 color) {
    uwm_ui_surface surface = pg_surface(app);
    uwm_uilib_fill_rect(&surface, x, y, w, h, color);
}

static void pg_stroke_rect(pg_app *app, int x, int y, int w, int h, pg_u32 color) {
    uwm_ui_surface surface = pg_surface(app);
    uwm_uilib_stroke_rect(&surface, x, y, w, h, color);
}

static void pg_draw_text_limit(pg_app *app, int x, int y, const char *text, int scale, pg_u32 color, int max_x) {
    uwm_ui_surface surface = pg_surface(app);
    uwm_uilib_draw_text_limit(&surface, x, y, text, scale, color, max_x);
}

static void pg_draw_text(pg_app *app, int x, int y, const char *text, int scale, pg_u32 color) {
    uwm_ui_surface surface = pg_surface(app);
    uwm_uilib_draw_text(&surface, x, y, text, scale, color);
}

static void pg_draw_button(pg_app *app, int x, int y, int w, int h, const char *label, int hot) {
    uwm_ui_surface surface = pg_surface(app);
    uwm_uilib_draw_button(&surface, x, y, w, h, label, PG_COLOR_BUTTON, PG_COLOR_BUTTON_HOT, PG_COLOR_TEXT,
                          PG_COLOR_BORDER, hot);
}

static void pg_draw_control_button(pg_app *app, int x, int kind) {
    uwm_ui_surface surface = pg_surface(app);
    uwm_uilib_draw_control_button(&surface, x, 0, PG_CONTROL_W, PG_TITLE_H, app->focused, kind, PG_COLOR_TEXT);
}

static int pg_present(pg_app *app) {
    uwm_ui_surface surface;

    if (app == (pg_app *)0 || app->window_id == 0ULL || app->pixels == (pg_u32 *)0) {
        return 0;
    }
    surface = pg_surface(app);
    return (uwm_uilib_present(app->window_id, &surface) != 0ULL) ? 1 : 0;
}

static int pg_list_w(const pg_app *app) {
    int list_w;

    if (app == (const pg_app *)0) {
        return 360;
    }
    list_w = (app->w * 56) / 100;
    list_w = pg_clampi(list_w, 340, app->w - 250);
    return list_w;
}

static int pg_visible_rows(const pg_app *app) {
    int top = PG_TITLE_H + PG_TOOLBAR_H + PG_HEADER_H;
    int bottom;
    int rows;

    if (app == (const pg_app *)0) {
        return 0;
    }
    bottom = app->h - PG_STATUS_H;
    rows = (bottom - top) / PG_ROW_H;
    return (rows > 0) ? rows : 0;
}

static void pg_clamp_selection(pg_app *app) {
    int rows;
    int max_scroll;

    if (app == (pg_app *)0) {
        return;
    }
    if (app->package_count == 0ULL) {
        app->selected = -1;
        app->scroll = 0;
        return;
    }
    if (app->selected < 0 || (u64)(unsigned int)app->selected >= app->package_count) {
        app->selected = 0;
    }
    rows = pg_visible_rows(app);
    if (rows <= 0) {
        app->scroll = 0;
        return;
    }
    max_scroll = ((int)app->package_count > rows) ? ((int)app->package_count - rows) : 0;
    app->scroll = pg_clampi(app->scroll, 0, max_scroll);
    if (app->selected < app->scroll) {
        app->scroll = app->selected;
    }
    if (app->selected >= app->scroll + rows) {
        app->scroll = app->selected - rows + 1;
    }
}

static int pg_resize_canvas(pg_app *app, int width, int height) {
    u64 count;
    pg_u32 *pixels;

    if (app == (pg_app *)0 || width <= 0 || height <= 0) {
        return 0;
    }
    count = (u64)(unsigned int)width * (u64)(unsigned int)height;
    if (count == 0ULL || count > PG_CANVAS_MAX_PIXELS || count > ((u64)-1) / 4ULL) {
        return 0;
    }
    pixels = (pg_u32 *)malloc((size_t)(count * 4ULL));
    if (pixels == (pg_u32 *)0) {
        return 0;
    }
    memset(pixels, 0, (size_t)(count * 4ULL));
    if (app->pixels != (pg_u32 *)0) {
        free(app->pixels);
    }
    app->pixels = pixels;
    app->pixel_count = count;
    app->w = width;
    app->h = height;
    return 1;
}

static int pg_hit_rect(int x, int y, int bx, int by, int bw, int bh) {
    return (x >= bx && x < bx + bw && y >= by && y < by + bh) ? 1 : 0;
}

static int pg_selected_valid(const pg_app *app) {
    return (app != (const pg_app *)0 && app->selected >= 0 && (u64)(unsigned int)app->selected < app->package_count) ? 1
                                                                                                                      : 0;
}

static void pg_refresh_installed(pg_app *app) {
    u64 len = 0ULL;
    u64 i;

    if (app == (pg_app *)0) {
        return;
    }
    for (i = 0ULL; i < app->package_count; i++) {
        app->installed[i] = 0;
        app->installed_versions[i][0] = '\0';
    }
    if (pkg_read_file(PKG_DB_PATH, pkg_db_buf, (u64)sizeof(pkg_db_buf), &len) == 0 || len == 0ULL) {
        return;
    }
    for (i = 0ULL; i < app->package_count; i++) {
        if (pkg_find_installed_version_in_db(pkg_db_buf, app->packages[i].name, app->installed_versions[i],
                                             (u64)sizeof(app->installed_versions[i])) != 0) {
            app->installed[i] = 1;
        }
    }
}

static void pg_status_from_api_error(pg_app *app, const char *fallback) {
    char error[128];

    if (pkg_json_get_string(pkg_text_buf, (const char *)0, "error", error, (u64)sizeof(error)) != 0 &&
        error[0] != '\0') {
        pg_set_status_name(app, "Remote error: ", error, "");
    } else {
        pg_set_status(app, fallback);
    }
}

static void pg_draw_titlebar(pg_app *app) {
    pg_u32 title_bg = (app->focused != 0) ? PG_COLOR_TITLE : PG_COLOR_TITLE_INACTIVE;
    pg_u32 title_fg = (app->focused != 0) ? PG_COLOR_WHITE : PG_COLOR_TEXT;

    pg_fill_rect(app, 0, 0, app->w, PG_TITLE_H, title_bg);
    pg_fill_rect(app, 0, PG_TITLE_H, app->w, 1, PG_COLOR_BORDER);
    pg_stroke_rect(app, 0, 0, app->w, app->h, PG_COLOR_BORDER);
    pg_draw_text_limit(app, 14, 12, "PACKAGE MANAGER", 1, title_fg, app->w - (PG_CONTROL_W * 3) - 8);
    pg_draw_control_button(app, app->w - (PG_CONTROL_W * 3), UWM_UI_CONTROL_MINIMIZE);
    pg_draw_control_button(app, app->w - (PG_CONTROL_W * 2),
                           app->maximized != 0 ? UWM_UI_CONTROL_RESTORE : UWM_UI_CONTROL_MAXIMIZE);
    pg_draw_control_button(app, app->w - PG_CONTROL_W, UWM_UI_CONTROL_CLOSE);
}

static int pg_search_w(const pg_app *app) {
    int search_w = app->w - 498;

    if (search_w < 150) {
        search_w = 150;
    }
    return search_w;
}

static int pg_button_x(const pg_app *app) {
    return 18 + pg_search_w(app) + 10;
}

static void pg_draw_toolbar(pg_app *app) {
    int y = PG_TITLE_H;
    int search_x = 18;
    int search_w = pg_search_w(app);
    int button_x = pg_button_x(app);

    pg_fill_rect(app, 0, y, app->w, PG_TOOLBAR_H, PG_COLOR_PANEL);
    pg_fill_rect(app, 0, y + PG_TOOLBAR_H - 1, app->w, 1, PG_COLOR_BORDER);
    pg_draw_text(app, 18, y + 8, "REMOTE PACKAGES", 1, PG_COLOR_MUTED);

    pg_fill_rect(app, search_x, y + 24, search_w, 24, PG_COLOR_INPUT);
    pg_stroke_rect(app, search_x, y + 24, search_w, 24, app->search_mode != 0 ? PG_COLOR_TITLE : PG_COLOR_BORDER);
    if (app->query[0] == '\0' && app->search_mode == 0) {
        pg_draw_text_limit(app, search_x + 10, y + 32, "SEARCH PACKAGE", 1, PG_COLOR_MUTED, search_x + search_w - 8);
    } else {
        pg_draw_text_limit(app, search_x + 10, y + 32, app->query, 1, PG_COLOR_TEXT, search_x + search_w - 16);
        if (app->search_mode != 0) {
            int cursor_x = search_x + 11 + ((int)ush_strlen(app->query) * 8);
            if (cursor_x < search_x + search_w - 8) {
                pg_fill_rect(app, cursor_x, y + 30, 1, 14, PG_COLOR_TEXT);
            }
        }
    }

    pg_draw_button(app, button_x, y + 20, 78, 30, "SEARCH", app->search_mode);
    pg_draw_button(app, button_x + 86, y + 20, 82, 30, "REFRESH", 0);
    pg_draw_button(app, button_x + 176, y + 20, 90, 30,
                   (pg_selected_valid(app) != 0 && app->installed[app->selected] != 0) ? "REINSTALL" : "INSTALL",
                   pg_selected_valid(app));
    pg_draw_button(app, button_x + 274, y + 20, 74, 30, "REMOVE",
                   (pg_selected_valid(app) != 0 && app->installed[app->selected] != 0) ? 1 : 0);
}

static void pg_draw_header(pg_app *app, int list_w) {
    int y = PG_TITLE_H + PG_TOOLBAR_H;

    pg_fill_rect(app, 0, y, list_w, PG_HEADER_H, 0x00EFEFEFU);
    pg_fill_rect(app, list_w, y, app->w - list_w, PG_HEADER_H, 0x00EFEFEFU);
    pg_fill_rect(app, 0, y + PG_HEADER_H - 1, app->w, 1, PG_COLOR_BORDER);
    pg_fill_rect(app, list_w - 1, y, 1, app->h - y - PG_STATUS_H, PG_COLOR_BORDER);
    pg_draw_text(app, 18, y + 9, "NAME", 1, PG_COLOR_MUTED);
    pg_draw_text(app, list_w - 188, y + 9, "VERSION", 1, PG_COLOR_MUTED);
    pg_draw_text(app, list_w - 92, y + 9, "STATE", 1, PG_COLOR_MUTED);
    pg_draw_text(app, list_w + 18, y + 9, "DETAILS", 1, PG_COLOR_MUTED);
}

static void pg_draw_rows(pg_app *app, int list_w) {
    int top = PG_TITLE_H + PG_TOOLBAR_H + PG_HEADER_H;
    int rows = pg_visible_rows(app);
    int i;

    pg_fill_rect(app, 0, top, list_w - 1, app->h - top - PG_STATUS_H, PG_COLOR_PANEL);
    if (app->package_count == 0ULL) {
        pg_draw_text(app, 24, top + 32, "NO PACKAGES", 2, PG_COLOR_MUTED);
        return;
    }

    for (i = 0; i < rows; i++) {
        int row_index = app->scroll + i;
        int y = top + (i * PG_ROW_H);
        const pkg_remote_package *pkg;
        pg_u32 bg;

        if (row_index < 0 || (u64)(unsigned int)row_index >= app->package_count) {
            break;
        }
        pkg = &app->packages[row_index];
        bg = (row_index == app->selected) ? PG_COLOR_SELECT : (((row_index & 1) != 0) ? PG_COLOR_ROW_ALT : PG_COLOR_PANEL);
        pg_fill_rect(app, 0, y, list_w - 1, PG_ROW_H, bg);
        if (row_index == app->selected) {
            pg_fill_rect(app, 0, y, 4, PG_ROW_H, PG_COLOR_SELECT_EDGE);
        }
        pg_fill_rect(app, 0, y + PG_ROW_H - 1, list_w - 1, 1, 0x00ECECECU);
        pg_draw_text_limit(app, 18, y + 9, pkg->name, 1, PG_COLOR_TEXT, list_w - 198);
        pg_draw_text_limit(app, list_w - 188, y + 9, pg_text_or_dash(pkg->version), 1, PG_COLOR_MUTED, list_w - 96);
        if (app->installed[row_index] != 0) {
            pg_fill_rect(app, list_w - 96, y + 5, 78, 18, PG_COLOR_INSTALLED);
            pg_stroke_rect(app, list_w - 96, y + 5, 78, 18, 0x00B7E0B7U);
            pg_draw_text(app, list_w - 88, y + 10, "INSTALLED", 1, PG_COLOR_TEXT);
        } else if (pkg->deprecated[0] != '\0') {
            pg_fill_rect(app, list_w - 96, y + 5, 78, 18, PG_COLOR_WARN);
            pg_stroke_rect(app, list_w - 96, y + 5, 78, 18, 0x00E7D48AU);
            pg_draw_text(app, list_w - 84, y + 10, "WARN", 1, PG_COLOR_TEXT);
        }
    }
}

static int pg_draw_detail_line(pg_app *app, int x, int y, const char *label, const char *value, pg_u32 value_color) {
    char line[256];

    line[0] = '\0';
    pg_append(line, (u64)sizeof(line), label);
    pg_append(line, (u64)sizeof(line), ": ");
    pg_append(line, (u64)sizeof(line), pg_text_or_dash(value));
    pg_draw_text_limit(app, x, y, line, 1, value_color, app->w - 18);
    return y + 20;
}

static void pg_draw_detail(pg_app *app, int list_w) {
    int x = list_w + 18;
    int y = PG_TITLE_H + PG_TOOLBAR_H + PG_HEADER_H + 20;
    const pkg_remote_package *pkg;
    char installed_line[96];

    pg_fill_rect(app, list_w, PG_TITLE_H + PG_TOOLBAR_H + PG_HEADER_H, app->w - list_w,
                 app->h - PG_TITLE_H - PG_TOOLBAR_H - PG_HEADER_H - PG_STATUS_H, PG_COLOR_PANEL_2);

    if (pg_selected_valid(app) == 0) {
        pg_draw_text(app, x, y + 12, "SELECT A PACKAGE", 2, PG_COLOR_MUTED);
        return;
    }

    pkg = &app->packages[app->selected];
    pg_draw_text_limit(app, x, y, pkg->name, 2, PG_COLOR_TEXT, app->w - 18);
    y += 34;

    installed_line[0] = '\0';
    if (app->installed[app->selected] != 0) {
        pg_append(installed_line, (u64)sizeof(installed_line), "yes");
        if (app->installed_versions[app->selected][0] != '\0') {
            pg_append(installed_line, (u64)sizeof(installed_line), " (");
            pg_append(installed_line, (u64)sizeof(installed_line), app->installed_versions[app->selected]);
            pg_append(installed_line, (u64)sizeof(installed_line), ")");
        }
    } else {
        pg_append(installed_line, (u64)sizeof(installed_line), "no");
    }

    y = pg_draw_detail_line(app, x, y, "version", pkg->version, PG_COLOR_TEXT);
    y = pg_draw_detail_line(app, x, y, "installed", installed_line,
                            app->installed[app->selected] != 0 ? 0x00008020U : PG_COLOR_MUTED);
    y = pg_draw_detail_line(app, x, y, "category", pkg->category, PG_COLOR_MUTED);
    y = pg_draw_detail_line(app, x, y, "tags", pkg->tags, PG_COLOR_MUTED);
    y = pg_draw_detail_line(app, x, y, "depends", pkg->depends, PG_COLOR_MUTED);
    y = pg_draw_detail_line(app, x, y, "size", pkg->size, PG_COLOR_MUTED);
    y = pg_draw_detail_line(app, x, y, "owner", pkg->owner, PG_COLOR_MUTED);
    y = pg_draw_detail_line(app, x, y, "target", pkg->target, PG_COLOR_MUTED);
    y += 8;
    pg_draw_text(app, x, y, "DESCRIPTION", 1, PG_COLOR_MUTED);
    y += 18;
    pg_draw_text_limit(app, x, y, pg_text_or_dash(pkg->description), 1, PG_COLOR_TEXT, app->w - 18);
    y += 34;
    if (pkg->deprecated[0] != '\0') {
        pg_fill_rect(app, x - 6, y - 8, app->w - x - 12, 34, PG_COLOR_WARN);
        pg_stroke_rect(app, x - 6, y - 8, app->w - x - 12, 34, 0x00E7D48AU);
        pg_draw_text_limit(app, x, y, pkg->deprecated, 1, PG_COLOR_TEXT, app->w - 18);
        y += 42;
    }
    if (pkg->sha256[0] != '\0') {
        pg_draw_text(app, x, y, "SHA256", 1, PG_COLOR_MUTED);
        y += 18;
        pg_draw_text_limit(app, x, y, pkg->sha256, 1, PG_COLOR_MUTED, app->w - 18);
    }
}

static void pg_draw_status(pg_app *app) {
    int y = app->h - PG_STATUS_H;
    char left[PG_STATUS_MAX];

    left[0] = '\0';
    pg_append(left, (u64)sizeof(left), app->status);
    if (app->repo[0] != '\0') {
        pg_append(left, (u64)sizeof(left), " | repo ");
        pg_append(left, (u64)sizeof(left), app->repo);
    }

    pg_fill_rect(app, 0, y, app->w, PG_STATUS_H, 0x00F7F7F7U);
    pg_fill_rect(app, 0, y, app->w, 1, PG_COLOR_BORDER);
    pg_draw_text_limit(app, 14, y + 10, left, 1, PG_COLOR_MUTED, app->w - 12);
}

static void pg_render(pg_app *app) {
    int list_w;

    if (app == (pg_app *)0 || app->pixels == (pg_u32 *)0) {
        return;
    }
    list_w = pg_list_w(app);
    pg_fill_rect(app, 0, 0, app->w, app->h, PG_COLOR_BG);
    pg_draw_titlebar(app);
    pg_draw_toolbar(app);
    pg_draw_header(app, list_w);
    pg_draw_rows(app, list_w);
    pg_draw_detail(app, list_w);
    pg_draw_status(app);
}

static void pg_render_present(pg_app *app) {
    pg_render(app);
    (void)pg_present(app);
}

static int pg_fetch_packages(pg_app *app, const char *api, const char *param_key, const char *param_value) {
    const char *cursor;
    pkg_remote_package package;
    u64 len = 0ULL;
    u64 count = 0ULL;

    if (app == (pg_app *)0 || api == (const char *)0) {
        return 0;
    }

    (void)pkg_load_repo(app->repo, (u64)sizeof(app->repo));
    if (param_value != (const char *)0 && param_value[0] != '\0') {
        pg_set_status_name(app, "Fetching: ", param_value, "");
    } else {
        pg_set_status(app, "Fetching remote package list");
    }
    pg_render_present(app);

    if (pkg_fetch_api(api, param_key, param_value, pkg_text_buf, (u64)sizeof(pkg_text_buf), &len) == 0) {
        (void)len;
        pg_set_status(app, "Remote fetch failed");
        pg_render_present(app);
        return 0;
    }

    cursor = pkg_json_find_named_array(pkg_text_buf, "packages");
    if (cursor == (const char *)0) {
        pg_status_from_api_error(app, "Invalid repository API response");
        app->package_count = 0ULL;
        app->selected = -1;
        app->scroll = 0;
        pg_render_present(app);
        return 0;
    }

    while (count < (u64)PG_MAX_PACKAGES && pkg_remote_next_package(&cursor, &package) != 0) {
        app->packages[count] = package;
        count++;
    }

    app->package_count = count;
    app->selected = (count > 0ULL) ? 0 : -1;
    app->scroll = 0;
    pg_refresh_installed(app);
    pg_clamp_selection(app);

    if (count == 0ULL) {
        pg_set_status(app, "No remote packages");
    } else if (count == (u64)PG_MAX_PACKAGES) {
        pg_set_status(app, "Loaded first 64 packages");
    } else {
        char tmp[64];
        (void)snprintf(tmp, sizeof(tmp), "Loaded %llu packages", (unsigned long long)count);
        pg_set_status(app, tmp);
    }
    pg_render_present(app);
    return 1;
}

static void pg_reload(pg_app *app) {
    app->search_mode = 0;
    app->query[0] = '\0';
    (void)pg_fetch_packages(app, "list", (const char *)0, (const char *)0);
}

static void pg_run_search(pg_app *app) {
    app->search_mode = 0;
    if (app->query[0] == '\0') {
        (void)pg_fetch_packages(app, "list", (const char *)0, (const char *)0);
        return;
    }
    (void)pg_fetch_packages(app, "search", "q", app->query);
}

static void pg_install_selected(pg_app *app) {
    ush_state sh;
    char arg[PKG_NAME_MAX + 16U];
    int ok;

    if (pg_selected_valid(app) == 0) {
        pg_set_status(app, "No package selected");
        return;
    }

    ush_init_state(&sh);
    pg_set_status_name(app, app->installed[app->selected] != 0 ? "Reinstalling " : "Installing ",
                       app->packages[app->selected].name, "");
    pg_render_present(app);

    if (app->installed[app->selected] != 0) {
        (void)snprintf(arg, sizeof(arg), "--reinstall %s", app->packages[app->selected].name);
        ok = pkg_cmd_install(&sh, arg);
    } else {
        ok = pkg_cmd_install(&sh, app->packages[app->selected].name);
    }
    pg_refresh_installed(app);
    pg_set_status_name(app, ok != 0 ? "Installed " : "Install failed: ", app->packages[app->selected].name, "");
    pg_render_present(app);
}

static void pg_remove_selected(pg_app *app) {
    int ok;

    if (pg_selected_valid(app) == 0) {
        pg_set_status(app, "No package selected");
        return;
    }
    if (app->installed[app->selected] == 0) {
        pg_set_status(app, "Selected package is not installed");
        return;
    }

    pg_set_status_name(app, "Removing ", app->packages[app->selected].name, "");
    pg_render_present(app);
    ok = pkg_cmd_remove(app->packages[app->selected].name);
    pg_refresh_installed(app);
    pg_set_status_name(app, ok != 0 ? "Removed " : "Remove failed: ", app->packages[app->selected].name, "");
    pg_render_present(app);
}

static void pg_select_delta(pg_app *app, int delta) {
    if (app == (pg_app *)0 || app->package_count == 0ULL) {
        return;
    }
    if (app->selected < 0) {
        app->selected = 0;
    } else {
        app->selected = pg_clampi(app->selected + delta, 0, (int)app->package_count - 1);
    }
    pg_clamp_selection(app);
}

static int pg_row_at(const pg_app *app, int local_x, int local_y) {
    int list_w;
    int top;
    int row;

    if (app == (const pg_app *)0) {
        return -1;
    }
    list_w = pg_list_w(app);
    top = PG_TITLE_H + PG_TOOLBAR_H + PG_HEADER_H;
    if (local_x < 0 || local_x >= list_w || local_y < top || local_y >= app->h - PG_STATUS_H) {
        return -1;
    }
    row = app->scroll + ((local_y - top) / PG_ROW_H);
    if (row < 0 || (u64)(unsigned int)row >= app->package_count) {
        return -1;
    }
    return row;
}

static void pg_toggle_maximize(pg_app *app) {
    cleonos_wm_move_req move_req;
    cleonos_wm_resize_req resize_req;
    int target_x;
    int target_y;
    int target_w;
    int target_h;

    if (app == (pg_app *)0 || app->window_id == 0ULL) {
        return;
    }

    if (app->maximized == 0) {
        app->restore_x = app->x;
        app->restore_y = app->y;
        app->restore_w = app->w;
        app->restore_h = app->h;
        target_x = 0;
        target_y = 24;
        target_w = app->screen_w;
        target_h = app->screen_h - 72;
    } else {
        target_x = app->restore_x;
        target_y = app->restore_y;
        target_w = app->restore_w;
        target_h = app->restore_h;
    }

    target_w = pg_clampi(target_w, PG_MIN_W, app->screen_w);
    target_h = pg_clampi(target_h, PG_MIN_H, app->screen_h);
    if (pg_resize_canvas(app, target_w, target_h) == 0) {
        pg_set_status(app, "Resize buffer allocation failed");
        return;
    }

    resize_req.window_id = app->window_id;
    resize_req.width = (u64)(unsigned int)target_w;
    resize_req.height = (u64)(unsigned int)target_h;
    (void)cleonos_sys_wm_resize(&resize_req);

    app->x = target_x;
    app->y = target_y;
    move_req.window_id = app->window_id;
    move_req.x = (u64)(i64)app->x;
    move_req.y = (u64)(i64)app->y;
    (void)cleonos_sys_wm_move(&move_req);
    app->maximized = (app->maximized == 0) ? 1 : 0;
    pg_clamp_selection(app);
    pg_render_present(app);
}

static void pg_handle_key(pg_app *app, u64 key) {
    if (app == (pg_app *)0) {
        return;
    }

    if (app->search_mode != 0) {
        if (key == 13ULL) {
            pg_run_search(app);
            return;
        }
        if (key == 27ULL) {
            app->search_mode = 0;
            pg_render_present(app);
            return;
        }
        if (key == 8ULL || key == 127ULL) {
            u64 len = ush_strlen(app->query);
            if (len > 0ULL) {
                app->query[len - 1ULL] = '\0';
            }
            pg_render_present(app);
            return;
        }
        if (key >= 32ULL && key <= 126ULL) {
            u64 len = ush_strlen(app->query);
            if (len + 1ULL < (u64)sizeof(app->query)) {
                app->query[len] = (char)key;
                app->query[len + 1ULL] = '\0';
            }
            pg_render_present(app);
            return;
        }
        return;
    }

    if (key == (u64)'q' || key == (u64)'Q' || key == 27ULL) {
        app->running = 0;
        return;
    }
    if (key == PG_KEY_UP || key == (u64)'k' || key == (u64)'K') {
        pg_select_delta(app, -1);
        pg_render_present(app);
        return;
    }
    if (key == PG_KEY_DOWN || key == (u64)'j' || key == (u64)'J') {
        pg_select_delta(app, 1);
        pg_render_present(app);
        return;
    }
    if (key == PG_KEY_LEFT) {
        pg_select_delta(app, -pg_visible_rows(app));
        pg_render_present(app);
        return;
    }
    if (key == PG_KEY_RIGHT) {
        pg_select_delta(app, pg_visible_rows(app));
        pg_render_present(app);
        return;
    }
    if (key == (u64)'r' || key == (u64)'R') {
        pg_reload(app);
        return;
    }
    if (key == (u64)'/' || key == (u64)'s' || key == (u64)'S') {
        app->search_mode = 1;
        pg_set_status(app, "Type query, Enter to search, Esc to cancel");
        pg_render_present(app);
        return;
    }
    if (key == (u64)'i' || key == (u64)'I' || key == 13ULL) {
        pg_install_selected(app);
        return;
    }
    if (key == (u64)'u' || key == (u64)'U' || key == 127ULL) {
        pg_remove_selected(app);
    }
}

static void pg_handle_toolbar_click(pg_app *app, int local_x, int local_y) {
    int y = PG_TITLE_H;
    int search_x = 18;
    int search_w = pg_search_w(app);
    int button_x = pg_button_x(app);

    if (pg_hit_rect(local_x, local_y, search_x, y + 24, search_w, 24) != 0) {
        app->search_mode = 1;
        pg_set_status(app, "Type query, Enter to search");
        pg_render_present(app);
        return;
    }
    if (pg_hit_rect(local_x, local_y, button_x, y + 20, 78, 30) != 0) {
        pg_run_search(app);
        return;
    }
    if (pg_hit_rect(local_x, local_y, button_x + 86, y + 20, 82, 30) != 0) {
        pg_reload(app);
        return;
    }
    if (pg_hit_rect(local_x, local_y, button_x + 176, y + 20, 90, 30) != 0) {
        pg_install_selected(app);
        return;
    }
    if (pg_hit_rect(local_x, local_y, button_x + 274, y + 20, 74, 30) != 0) {
        pg_remove_selected(app);
    }
}

static void pg_handle_mouse_button(pg_app *app, const cleonos_wm_event *event) {
    u64 buttons;
    u64 changed;
    int local_x;
    int local_y;
    int left_changed;
    int left_down;
    int row;

    if (app == (pg_app *)0 || event == (const cleonos_wm_event *)0) {
        return;
    }
    buttons = event->arg0;
    changed = event->arg1;
    local_x = pg_u64_as_i32(event->arg2);
    local_y = pg_u64_as_i32(event->arg3);
    left_changed = ((changed & 0x1ULL) != 0ULL) ? 1 : 0;
    left_down = ((buttons & 0x1ULL) != 0ULL) ? 1 : 0;

    if (left_changed == 0) {
        return;
    }
    if (left_down == 0) {
        app->dragging = 0;
        return;
    }

    if (local_y >= 0 && local_y < PG_TITLE_H) {
        if (local_x >= app->w - PG_CONTROL_W) {
            app->running = 0;
            return;
        }
        if (local_x >= app->w - (PG_CONTROL_W * 2) && local_x < app->w - PG_CONTROL_W) {
            pg_toggle_maximize(app);
            return;
        }
        if (local_x >= app->w - (PG_CONTROL_W * 3) && local_x < app->w - (PG_CONTROL_W * 2)) {
            pg_set_status(app, "Minimize is handled by UWM taskbar for external windows");
            pg_render_present(app);
            return;
        }
        app->dragging = 1;
        app->drag_dx = local_x;
        app->drag_dy = local_y;
        return;
    }

    if (local_y >= PG_TITLE_H && local_y < PG_TITLE_H + PG_TOOLBAR_H) {
        pg_handle_toolbar_click(app, local_x, local_y);
        return;
    }

    row = pg_row_at(app, local_x, local_y);
    if (row >= 0) {
        app->selected = row;
        app->search_mode = 0;
        pg_clamp_selection(app);
        pg_render_present(app);
    }
}

static void pg_handle_mouse_move(pg_app *app, const cleonos_wm_event *event) {
    cleonos_wm_move_req req;

    if (app == (pg_app *)0 || event == (const cleonos_wm_event *)0 || app->dragging == 0 || app->maximized != 0) {
        return;
    }
    app->x = pg_u64_as_i32(event->arg0) - app->drag_dx;
    app->y = pg_u64_as_i32(event->arg1) - app->drag_dy;
    app->x = pg_clampi(app->x, 0, app->screen_w - app->w);
    app->y = pg_clampi(app->y, 24, app->screen_h - 40);
    req.window_id = app->window_id;
    req.x = (u64)(i64)app->x;
    req.y = (u64)(i64)app->y;
    (void)cleonos_sys_wm_move(&req);
}

static void pg_loop(pg_app *app) {
    while (app->running != 0) {
        u64 budget = 0ULL;
        int handled = 0;

        while (budget < PG_EVENT_BUDGET) {
            cleonos_wm_event event;
            memset(&event, 0, sizeof(event));
            if (cleonos_sys_wm_poll_event(app->window_id, &event) == 0ULL) {
                break;
            }
            handled = 1;
            if (event.type == CLEONOS_WM_EVENT_FOCUS_GAINED) {
                app->focused = 1;
                pg_render_present(app);
            } else if (event.type == CLEONOS_WM_EVENT_FOCUS_LOST) {
                app->focused = 0;
                app->dragging = 0;
                pg_render_present(app);
            } else if (event.type == CLEONOS_WM_EVENT_KEY) {
                pg_handle_key(app, event.arg0);
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_BUTTON) {
                pg_handle_mouse_button(app, &event);
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_MOVE) {
                pg_handle_mouse_move(app, &event);
            }
            if (app->running == 0) {
                break;
            }
            budget++;
        }

        if (app->running == 0) {
            break;
        }
        if (handled != 0 || app->dragging != 0) {
            (void)cleonos_sys_yield();
        } else {
            (void)cleonos_sys_sleep_ticks(1ULL);
        }
    }
}

static int pg_choose_geometry(pg_app *app) {
    cleonos_display_info display;
    int max_w;
    int max_h;

    memset(&display, 0, sizeof(display));
    if (app == (pg_app *)0 || cleonos_sys_display_info(CLEONOS_DISPLAY_TARGET_WM, &display) == 0ULL ||
        display.logical_width == 0ULL || display.logical_height == 0ULL || display.logical_width > 4096ULL ||
        display.logical_height > 4096ULL) {
        return 0;
    }
    app->screen_w = (int)display.logical_width;
    app->screen_h = (int)display.logical_height;
    max_w = app->screen_w - 80;
    max_h = app->screen_h - 120;
    if (max_w < PG_MIN_W) {
        max_w = app->screen_w;
    }
    if (max_h < PG_MIN_H) {
        max_h = app->screen_h;
    }
    app->w = pg_clampi(PG_DEFAULT_W, PG_MIN_W, max_w);
    app->h = pg_clampi(PG_DEFAULT_H, PG_MIN_H, max_h);
    app->x = (app->screen_w > app->w) ? ((app->screen_w - app->w) / 2) : 0;
    app->y = (app->screen_h > app->h) ? ((app->screen_h - app->h) / 2) : 24;
    if (app->y < 24) {
        app->y = 24;
    }
    return 1;
}

static int pg_create_window(pg_app *app) {
    cleonos_wm_create_req req;

    if (pg_resize_canvas(app, app->w, app->h) == 0) {
        return 0;
    }

    app->old_tty = cleonos_sys_tty_active();
    if (app->old_tty != PG_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(PG_TTY_DISPLAY);
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
    app->focused = 1;
    (void)cleonos_sys_wm_set_focus(app->window_id);
    return 1;
}

static void pg_destroy(pg_app *app) {
    if (app == (pg_app *)0) {
        return;
    }
    if (app->window_id != 0ULL) {
        (void)cleonos_sys_wm_destroy(app->window_id);
        app->window_id = 0ULL;
    }
    if (app->pixels != (pg_u32 *)0) {
        free(app->pixels);
        app->pixels = (pg_u32 *)0;
    }
    if (app->tty_switched != 0) {
        (void)cleonos_sys_tty_switch(app->old_tty);
        app->tty_switched = 0;
    }
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    pg_app *app = &pg_state;

    (void)argc;
    (void)argv;
    (void)envp;

    memset(app, 0, sizeof(*app));
    app->running = 1;
    app->selected = -1;
    pg_set_status(app, "Starting package manager");

    if (pg_choose_geometry(app) == 0 || pg_create_window(app) == 0) {
        pg_destroy(app);
        return 1;
    }

    pg_render_present(app);
    pg_reload(app);
    pg_loop(app);
    pg_destroy(app);
    return 0;
}
