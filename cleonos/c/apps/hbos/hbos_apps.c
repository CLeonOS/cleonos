#include "hbos.h"

static void hbos_app_split_arg(const char *cmdline, char *arg, hbos_u32 arg_size) {
    hbos_u32 i = 0U;
    hbos_u32 p = 0U;

    if (arg == (char *)0 || arg_size == 0U) {
        return;
    }
    arg[0] = '\0';
    if (cmdline == (const char *)0) {
        return;
    }
    while (cmdline[i] > ' ') {
        i++;
    }
    while (cmdline[i] == ' ' || cmdline[i] == '\t') {
        i++;
    }
    while (cmdline[i] != '\0' && p + 1U < arg_size) {
        arg[p++] = cmdline[i++];
    }
    arg[p] = '\0';
}

static int hbos_app_hello3(hbos_state *state, const char *args) {
    (void)args;
    hbos_put_history(state, "hello", HBOS_COLOR_CONSOLE_TEXT);
    return 0;
}

static int hbos_app_hello_string(hbos_state *state, const char *args) {
    (void)args;
    hbos_put_history(state, "hello, world", HBOS_COLOR_CONSOLE_TEXT);
    return 0;
}

static int hbos_app_a(hbos_state *state, const char *args) {
    (void)args;
    hbos_put_history(state, "A", HBOS_COLOR_CONSOLE_TEXT);
    return 0;
}

static int hbos_app_stars(hbos_state *state, const char *args) {
    int i;
    (void)args;
    hbos_put_history(state, "[stars] window opened; press Enter in real HariboteOS to close", HBOS_COLOR_CONSOLE_DIM);
    for (i = 0; i < 80; i++) {
        int x = 250 + ((i * 37) % 300);
        int y = 30 + ((i * 53) % 90);
        hbos_video_rect(state, x, y, 2, 2, 0x00FFFFFFU);
    }
    state->dirty = 1;
    return 0;
}

static int hbos_app_lines(hbos_state *state, const char *args) {
    int i;
    (void)args;
    hbos_put_history(state, "[lines] window opened; press Enter in real HariboteOS to close", HBOS_COLOR_CONSOLE_DIM);
    for (i = 0; i < 12; i++) {
        hbos_video_rect(state, 252 + i * 18, 92 + i, 120 - i * 4, 2, 0x0000FF00U + (hbos_u32)(i * 0x00100000U));
        hbos_video_rect(state, 252 + i * 18, 94 + i, 2, 46, 0x0000FF00U + (hbos_u32)(i * 0x00001000U));
    }
    state->dirty = 1;
    return 0;
}

static int hbos_app_noodle(hbos_state *state, const char *args) {
    hbos_u32 i;
    (void)args;
    hbos_put_history(state, "[noodle] window timer demo", HBOS_COLOR_CONSOLE_DIM);
    for (i = 0U; i < 3U; i++) {
        hbos_put_history_fmt_u32(state, "  noodle timer ", i + 1U, "/3", HBOS_COLOR_CONSOLE_DIM);
        hbos_present(state);
        hbos_sleep(1000ULL);
    }
    hbos_put_history(state, "[noodle] done", HBOS_COLOR_CONSOLE_DIM);
    return 0;
}

static int hbos_app_winhello(hbos_state *state, const char *args) {
    (void)args;
    hbos_put_history(state, "[hello] window opened", HBOS_COLOR_CONSOLE_DIM);
    state->show_about = 1;
    state->dirty = 1;
    return 0;
}

static int hbos_app_type(hbos_state *state, const char *args) {
    char path[32];
    const hbos_file *file;

    hbos_app_split_arg(args, path, (hbos_u32)sizeof(path));
    file = hbos_find_file(path);
    if (file == (const hbos_file *)0) {
        hbos_put_history(state, "File not found.", HBOS_COLOR_CONSOLE_TEXT);
        return 0;
    }

    hbos_put_history(state, hbos_file_content_for_state(state, file), HBOS_COLOR_CONSOLE_TEXT);
    return 0;
}

static int hbos_app_chklang(hbos_state *state, const char *args) {
    (void)args;
    hbos_put_history(state, "English ASCII mode", HBOS_COLOR_CONSOLE_TEXT);
    return 0;
}

static int hbos_app_iroha(hbos_state *state, const char *args) {
    (void)args;
    hbos_put_history(state, "[iroha] Japanese text output requires Haribote font encoding", HBOS_COLOR_CONSOLE_TEXT);
    return 0;
}

static const hbos_app hbos_apps[] = {
    {"A", "A.HRB", "prints a single letter", hbos_app_a},
    {"HELLO3", "HELLO3.HRB", "hello world HRB app", hbos_app_hello3},
    {"HELLO4", "HELLO4.HRB", "hello world string HRB app", hbos_app_hello_string},
    {"HELLO5", "HELLO5.HRB", "hello world assembly HRB app", hbos_app_hello_string},
    {"WINHELO", "WINHELO.HRB", "opens a simple window", hbos_app_winhello},
    {"WINHELO2", "WINHELO2.HRB", "opens a hello window", hbos_app_winhello},
    {"WINHELO3", "WINHELO3.HRB", "opens a hello window", hbos_app_winhello},
    {"STARS", "STARS.HRB", "star field graphics demo", hbos_app_stars},
    {"LINES", "LINES.HRB", "line drawing graphics demo", hbos_app_lines},
    {"NOODLE", "NOODLE.HRB", "timer demo", hbos_app_noodle},
    {"TYPE", "TYPE.HRB", "prints a file", hbos_app_type},
    {"CHKLANG", "CHKLANG.HRB", "prints language mode", hbos_app_chklang},
    {"IROHA", "IROHA.HRB", "prints sample Japanese text", hbos_app_iroha},
};

const hbos_app *hbos_app_at(hbos_u32 index) {
    if (index >= hbos_app_count()) {
        return (const hbos_app *)0;
    }
    return &hbos_apps[index];
}

hbos_u32 hbos_app_count(void) {
    return (hbos_u32)(sizeof(hbos_apps) / sizeof(hbos_apps[0]));
}

const hbos_app *hbos_find_app(const char *name) {
    char cmd[32];
    hbos_u32 i;
    hbos_split_first(name, cmd, (hbos_u32)sizeof(cmd), (const char **)0);
    for (i = 0U; i < hbos_app_count(); i++) {
        if (hbos_streq_ci(cmd, hbos_apps[i].name) != 0 || hbos_streq_ci(cmd, hbos_apps[i].display_name) != 0) {
            return &hbos_apps[i];
        }
    }
    return (const hbos_app *)0;
}

int hbos_run_builtin_app(hbos_state *state, const hbos_app *app, const char *cmdline) {
    if (state == (hbos_state *)0 || app == (const hbos_app *)0 || app->entry == (hbos_app_entry)0) {
        return 0;
    }

    (void)app->entry(state, cmdline);
    return 1;
}
