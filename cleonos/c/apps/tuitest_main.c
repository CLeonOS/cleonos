#include <cleonos_tui.h>
#include <dlfcn.h>
#include <stdio.h>

#define TUITEST_LIB_PATH "/shell/tui.elf"

typedef void (*tui_void_fn)(void);
typedef void (*tui_style_fn)(cleonos_tui_style);
typedef void (*tui_puts_at_fn)(int, int, const char *);
typedef void (*tui_rect_fn)(cleonos_tui_rect, char, cleonos_tui_style);
typedef void (*tui_box_fn)(cleonos_tui_rect, const char *, cleonos_tui_style);
typedef void (*tui_button_fn)(cleonos_tui_rect, const char *, int);
typedef void (*tui_status_fn)(int, const char *, const char *);
typedef int (*tui_read_key_fn)(void);
typedef const char *(*tui_key_name_fn)(int, char *, tui_u64);
typedef cleonos_tui_style (*tui_make_style_fn)(int, int, unsigned int);

typedef struct tuitest_api {
    tui_void_fn init;
    tui_void_fn shutdown;
    tui_void_fn clear;
    tui_void_fn reset_style;
    tui_void_fn refresh;
    tui_style_fn set_style;
    tui_puts_at_fn puts_at;
    tui_rect_fn fill_rect;
    tui_box_fn box;
    tui_button_fn button;
    tui_status_fn status_bar;
    tui_read_key_fn read_key;
    tui_key_name_fn key_name;
    tui_make_style_fn make_style;
} tuitest_api;

static int tuitest_load_symbol(void *handle, const char *name, void **out) {
    *out = dlsym(handle, name);
    if (*out == (void *)0) {
        (void)printf("[tuitest] missing symbol: %s\n", name);
        return 0;
    }
    return 1;
}

static int tuitest_load_api(void *handle, tuitest_api *api) {
    return tuitest_load_symbol(handle, "cleonos_tui_init", (void **)&api->init) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_shutdown", (void **)&api->shutdown) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_clear", (void **)&api->clear) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_reset_style", (void **)&api->reset_style) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_refresh", (void **)&api->refresh) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_set_style", (void **)&api->set_style) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_puts_at", (void **)&api->puts_at) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_fill_rect", (void **)&api->fill_rect) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_box", (void **)&api->box) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_button", (void **)&api->button) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_status_bar", (void **)&api->status_bar) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_read_key", (void **)&api->read_key) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_key_name", (void **)&api->key_name) != 0 &&
           tuitest_load_symbol(handle, "cleonos_tui_make_style", (void **)&api->make_style) != 0;
}

static void tuitest_print_line(const tuitest_api *tui, cleonos_tui_style style, const char *text) {
    tui->set_style(style);
    (void)puts(text);
    tui->reset_style();
}

static void tuitest_draw_button_line(const tuitest_api *tui, int focused_button) {
    cleonos_tui_style focused = tui->make_style(CLEONOS_TUI_COLOR_BLACK, CLEONOS_TUI_COLOR_CYAN, CLEONOS_TUI_ATTR_BOLD);
    cleonos_tui_style normal = tui->make_style(CLEONOS_TUI_COLOR_WHITE, CLEONOS_TUI_COLOR_BLUE, 0U);

    (void)printf("|   ");
    tui->set_style((focused_button == 0) ? focused : normal);
    (void)printf("  OK  ");
    tui->reset_style();
    (void)printf("      ");
    tui->set_style((focused_button == 1) ? focused : normal);
    (void)printf(" Cancel ");
    tui->reset_style();
    (void)printf("      ");
    tui->set_style((focused_button == 2) ? focused : normal);
    (void)printf(" Apply ");
    tui->reset_style();
    (void)puts("                          |");
}

static void tuitest_draw(const tuitest_api *tui, int focused_button, const char *last_key) {
    cleonos_tui_style border =
        tui->make_style(CLEONOS_TUI_COLOR_CYAN, CLEONOS_TUI_COLOR_DEFAULT, CLEONOS_TUI_ATTR_BOLD);
    cleonos_tui_style title =
        tui->make_style(CLEONOS_TUI_COLOR_YELLOW, CLEONOS_TUI_COLOR_DEFAULT, CLEONOS_TUI_ATTR_BOLD);
    cleonos_tui_style bar = tui->make_style(CLEONOS_TUI_COLOR_BLACK, CLEONOS_TUI_COLOR_CYAN, CLEONOS_TUI_ATTR_BOLD);

    tui->clear();
    tuitest_print_line(tui, border, "+------------------------------------------------------------------------------+");
    tuitest_print_line(tui, title, "| CLeonOS TUI dynamic library                                                   |");
    tuitest_print_line(tui, border, "+------------------------------------------------------------------------------+");
    (void)puts("|                                                                              |");
    (void)puts("| libtui-style API loaded through dlopen/dlsym.                                 |");
    (void)puts("| This fallback-safe demo avoids heavy cursor positioning on weak TTY backends. |");
    (void)puts("|                                                                              |");
    (void)puts("| Features: colors, boxes, status bars, buttons and blocking key read.          |");
    (void)puts("| Keys: TAB switches focus, q exits. Other keys are displayed below.            |");
    (void)puts("|                                                                              |");
    tuitest_draw_button_line(tui, focused_button);
    (void)puts("|                                                                              |");
    (void)printf("| Last key: %-66s |\n", last_key);
    (void)puts("|                                                                              |");
    tuitest_print_line(tui, border, "+------------------------------------------------------------------------------+");
    tui->set_style(bar);
    (void)puts(" tuitest | dynamic TUI library demo                         TAB focus | q quit ");
    tui->reset_style();
    tui->refresh();
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    const char *lib_path = TUITEST_LIB_PATH;
    void *handle;
    tuitest_api tui;
    int focused_button = 0;
    int running = 1;
    char key_name[32];

    (void)envp;

    if (argc > 1 && argv != (char **)0 && argv[1] != (char *)0 && argv[1][0] != '\0') {
        lib_path = argv[1];
    }

    handle = dlopen(lib_path, 0);
    if (handle == (void *)0) {
        (void)printf("[tuitest] dlopen failed: %s\n", lib_path);
        return 1;
    }

    if (tuitest_load_api(handle, &tui) == 0) {
        (void)dlclose(handle);
        return 2;
    }

    key_name[0] = '-';
    key_name[1] = '\0';
    tui.init();

    while (running != 0) {
        int key;

        tuitest_draw(&tui, focused_button, key_name);
        key = tui.read_key();
        (void)tui.key_name(key, key_name, (tui_u64)sizeof(key_name));

        if (key == 'q' || key == 'Q' || key == 27) {
            running = 0;
        } else if (key == '\t') {
            focused_button = (focused_button + 1) % 3;
        }
    }

    tui.shutdown();
    (void)dlclose(handle);
    (void)puts("[tuitest] PASS");
    return 0;
}
