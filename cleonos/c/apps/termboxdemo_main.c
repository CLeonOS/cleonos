#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include <termbox2.h>
#include "cmd_runtime.h"

#define TERMBOX2_LIB_PATH "/shell/termbox2.elf"

typedef int (*tb_init_fn)(void);
typedef int (*tb_shutdown_fn)(void);
typedef int (*tb_width_fn)(void);
typedef int (*tb_height_fn)(void);
typedef int (*tb_clear_fn)(void);
typedef int (*tb_present_fn)(void);
typedef int (*tb_set_cursor_fn)(int, int);
typedef int (*tb_hide_cursor_fn)(void);
typedef int (*tb_set_cell_fn)(int, int, uint32_t, uintattr_t, uintattr_t);
typedef int (*tb_poll_event_fn)(struct tb_event *);
typedef int (*tb_printf_fn)(int, int, uintattr_t, uintattr_t, const char *, ...);
typedef const char *(*tb_version_fn)(void);

typedef struct termbox_api {
    tb_init_fn init;
    tb_shutdown_fn shutdown;
    tb_width_fn width;
    tb_height_fn height;
    tb_clear_fn clear;
    tb_present_fn present;
    tb_set_cursor_fn set_cursor;
    tb_hide_cursor_fn hide_cursor;
    tb_set_cell_fn set_cell;
    tb_poll_event_fn poll_event;
    tb_printf_fn print;
    tb_version_fn version;
} termbox_api;

static int termboxdemo_load_symbol(void *handle, const char *name, void **out) {
    *out = dlsym(handle, name);
    if (*out == (void *)0) {
        (void)printf((ush_locale_is_zh() != 0) ? "[termboxdemo] 缺少符号: %s\n"
                                               : "[termboxdemo] missing symbol: %s\n",
                     name);
        return 0;
    }
    return 1;
}

static int termboxdemo_load_api(void *handle, termbox_api *api) {
    memset(api, 0, sizeof(*api));
    return termboxdemo_load_symbol(handle, "tb_init", (void **)&api->init) != 0 &&
           termboxdemo_load_symbol(handle, "tb_shutdown", (void **)&api->shutdown) != 0 &&
           termboxdemo_load_symbol(handle, "tb_width", (void **)&api->width) != 0 &&
           termboxdemo_load_symbol(handle, "tb_height", (void **)&api->height) != 0 &&
           termboxdemo_load_symbol(handle, "tb_clear", (void **)&api->clear) != 0 &&
           termboxdemo_load_symbol(handle, "tb_present", (void **)&api->present) != 0 &&
           termboxdemo_load_symbol(handle, "tb_set_cursor", (void **)&api->set_cursor) != 0 &&
           termboxdemo_load_symbol(handle, "tb_hide_cursor", (void **)&api->hide_cursor) != 0 &&
           termboxdemo_load_symbol(handle, "tb_set_cell", (void **)&api->set_cell) != 0 &&
           termboxdemo_load_symbol(handle, "tb_poll_event", (void **)&api->poll_event) != 0 &&
           termboxdemo_load_symbol(handle, "tb_printf", (void **)&api->print) != 0 &&
           termboxdemo_load_symbol(handle, "tb_version", (void **)&api->version) != 0;
}

static void termboxdemo_draw_border(const termbox_api *tb, int w, int h) {
    int x;
    int y;
    uintattr_t border_fg = TB_CYAN | TB_BOLD;

    for (x = 0; x < w; x++) {
        (void)tb->set_cell(x, 0, '-', border_fg, TB_DEFAULT);
        (void)tb->set_cell(x, h - 1, '-', border_fg, TB_DEFAULT);
    }
    for (y = 0; y < h; y++) {
        (void)tb->set_cell(0, y, '|', border_fg, TB_DEFAULT);
        (void)tb->set_cell(w - 1, y, '|', border_fg, TB_DEFAULT);
    }
    (void)tb->set_cell(0, 0, '+', border_fg, TB_DEFAULT);
    (void)tb->set_cell(w - 1, 0, '+', border_fg, TB_DEFAULT);
    (void)tb->set_cell(0, h - 1, '+', border_fg, TB_DEFAULT);
    (void)tb->set_cell(w - 1, h - 1, '+', border_fg, TB_DEFAULT);
}

static void termboxdemo_draw(const termbox_api *tb, int focus_x, int focus_y, const struct tb_event *last) {
    int w = tb->width();
    int h = tb->height();
    char event_buf[128];
    const char *title =
        (ush_locale_is_zh() != 0) ? "CLeonOS termbox2 移植测试" : "CLeonOS termbox2 port test";

    if (w < 10 || h < 6) {
        return;
    }

    (void)tb->clear();
    termboxdemo_draw_border(tb, w, h);
    (void)tb->print(2, 1, TB_YELLOW | TB_BOLD, TB_DEFAULT, "%s", title);
    (void)tb->print(2, 3, TB_WHITE, TB_DEFAULT, "version: %s", tb->version());
    (void)tb->print(2, 4, TB_WHITE, TB_DEFAULT, "size: %dx%d", w, h);
    (void)tb->print(2, 5, TB_WHITE, TB_DEFAULT,
                    (ush_locale_is_zh() != 0) ? "方向键移动光标，q/ESC 退出" : "Arrows move cursor, q/ESC quits");

    if (last != (const struct tb_event *)0) {
        (void)snprintf(event_buf, sizeof(event_buf), "type=%u key=%u ch=U+%04X mod=%u x=%d y=%d",
                       (unsigned int)last->type, (unsigned int)last->key, (unsigned int)last->ch,
                       (unsigned int)last->mod, (int)last->x, (int)last->y);
        (void)tb->print(2, 7, TB_GREEN, TB_DEFAULT, "%s", event_buf);
    }

    if (focus_x < 1) {
        focus_x = 1;
    }
    if (focus_y < 1) {
        focus_y = 1;
    }
    if (focus_x >= w - 1) {
        focus_x = w - 2;
    }
    if (focus_y >= h - 1) {
        focus_y = h - 2;
    }

    (void)tb->set_cell(focus_x, focus_y, '@', TB_BLACK | TB_BOLD, TB_CYAN);
    (void)tb->set_cursor(focus_x, focus_y);
    (void)tb->present();
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    const char *lib_path = TERMBOX2_LIB_PATH;
    void *handle;
    termbox_api tb;
    struct tb_event ev;
    int focus_x = 10;
    int focus_y = 10;
    int running = 1;

    (void)envp;

    if (argc > 1 && argv != (char **)0 && argv[1] != (char *)0 && argv[1][0] != '\0') {
        lib_path = argv[1];
    }

    handle = dlopen(lib_path, 0);
    if (handle == (void *)0) {
        (void)printf((ush_locale_is_zh() != 0) ? "[termboxdemo] dlopen 失败: %s\n"
                                               : "[termboxdemo] dlopen failed: %s\n",
                     lib_path);
        return 1;
    }

    if (termboxdemo_load_api(handle, &tb) == 0) {
        (void)dlclose(handle);
        return 2;
    }

    if (tb.init() != TB_OK) {
        (void)printf((ush_locale_is_zh() != 0) ? "[termboxdemo] tb_init 失败\n"
                                               : "[termboxdemo] tb_init failed\n");
        (void)dlclose(handle);
        return 3;
    }

    memset(&ev, 0, sizeof(ev));
    while (running != 0) {
        termboxdemo_draw(&tb, focus_x, focus_y, &ev);
        memset(&ev, 0, sizeof(ev));
        if (tb.poll_event(&ev) < 0) {
            continue;
        }

        if (ev.type == TB_EVENT_RESIZE) {
            if (focus_x >= ev.w - 1) {
                focus_x = ev.w - 2;
            }
            if (focus_y >= ev.h - 1) {
                focus_y = ev.h - 2;
            }
            continue;
        }

        if (ev.type != TB_EVENT_KEY) {
            continue;
        }

        if (ev.key == TB_KEY_ESC || ev.ch == 'q' || ev.ch == 'Q') {
            running = 0;
        } else if (ev.key == TB_KEY_ARROW_LEFT) {
            focus_x--;
        } else if (ev.key == TB_KEY_ARROW_RIGHT) {
            focus_x++;
        } else if (ev.key == TB_KEY_ARROW_UP) {
            focus_y--;
        } else if (ev.key == TB_KEY_ARROW_DOWN) {
            focus_y++;
        }
    }

    (void)tb.shutdown();
    (void)dlclose(handle);
    ush_writeln_i18n("[termboxdemo] PASS", "[termboxdemo] 通过");
    return 0;
}
