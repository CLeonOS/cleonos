#include "uwm.h"

static void ush_uwm_usage(void) {
    ush_writeln("usage: uwm");
    ush_writeln("keys: q quit, tab focus, 1/2/3 restore, wasd/arrow move");
    ush_writeln("keys: m minimize, x close, t pin top, +/- resize");
    ush_writeln("mouse: drag titlebar, resize bottom-right, use taskbar/start");
}

static int ush_uwm_parse_args(const char *arg) {
    char first[USH_PATH_MAX];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == 0) {
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (rest != (const char *)0 && rest[0] != 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    return 0;
}

static int ush_uwm_fail(ush_uwm_session *sess, const char *message) {
    if (sess != (ush_uwm_session *)0 && message != (const char *)0) {
        ush_copy(sess->last_error, (u64)sizeof(sess->last_error), message);
    }

    return 0;
}

static int ush_uwm_work_bottom(const ush_uwm_session *sess) {
    int bottom;

    if (sess == (const ush_uwm_session *)0) {
        return USH_UWM_TOP_CLAMP_Y;
    }

    bottom = sess->screen_h - USH_UWM_TASKBAR_H;
    if (bottom < USH_UWM_TOP_CLAMP_Y) {
        bottom = USH_UWM_TOP_CLAMP_Y;
    }

    return bottom;
}

static int ush_uwm_fit_dimension(int wanted, int min_value, int max_value) {
    if (max_value < 64) {
        return 64;
    }

    if (max_value < min_value) {
        return max_value;
    }

    return ush_uwm_clampi(wanted, min_value, max_value);
}

static void ush_uwm_init_window(ush_uwm_window *win, ush_uwm_window_kind kind, const char *title, const char *subtitle,
                                int x, int y, int w, int h, ush_uwm_u32 accent, int topmost, int closed) {
    if (win == (ush_uwm_window *)0) {
        return;
    }

    win->id = 0ULL;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->pixels = (ush_uwm_u32 *)0;
    win->pixel_count = 0ULL;
    win->alive = 0;
    win->minimized = 0;
    win->closed = closed;
    win->topmost = topmost;
    win->dirty = 1;
    win->kind = kind;
    win->accent = accent;
    ush_copy(win->title, (u64)sizeof(win->title), title);
    ush_copy(win->subtitle, (u64)sizeof(win->subtitle), subtitle);
}

static int ush_uwm_boot_window(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;

    if (sess == (ush_uwm_session *)0 || ush_uwm_window_index_valid(index) == 0) {
        return 0;
    }

    win = &sess->windows[index];
    if (win->closed != 0) {
        return 0;
    }

    if (win->pixels == (ush_uwm_u32 *)0 && ush_uwm_alloc_pixels(win) == 0) {
        return 0;
    }

    ush_uwm_render_window(sess, index);
    if (ush_uwm_create_window(win) == 0) {
        return 0;
    }

    if (ush_uwm_present_window(win) == 0) {
        ush_uwm_destroy_kernel_window(win);
        return 0;
    }

    return 1;
}

int ush_uwm_prepare_session(ush_uwm_session *sess) {
    cleonos_fb_info fb;
    int work_bottom;
    int work_h;
    int base_w;
    int base_h;
    int start_w;
    int start_h;
    int app_gap;
    int i;
    const char *titles[USH_UWM_APP_COUNT] = {"FILE EXPLORER", "NOTEPAD", "EDGE"};
    const char *subtitles[USH_UWM_APP_COUNT] = {"LOCAL DISK AND SYSTEM FILES", "EDIT TEXT INSIDE CLEONOS",
                                                "WEB PREVIEW AND HTTP TOOLS"};
    const ush_uwm_u32 accents[USH_UWM_APP_COUNT] = {0x000078D7U, 0x0000A300U, 0x00007ACCU};

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    ush_zero(sess, (u64)sizeof(*sess));
    ush_copy(sess->last_error, (u64)sizeof(sess->last_error), "uwm: init failed");

    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL) {
        return ush_uwm_fail(sess, "uwm: framebuffer unavailable");
    }

    if (fb.width > 4096ULL || fb.height > 4096ULL) {
        return ush_uwm_fail(sess, "uwm: framebuffer is larger than 4096x4096");
    }

    sess->screen_w = (int)fb.width;
    sess->screen_h = (int)fb.height;
    sess->active_window = 0;
    sess->drag_window = -1;
    sess->resize_window = -1;
    sess->tty_before = cleonos_sys_tty_active();

    work_bottom = ush_uwm_work_bottom(sess);
    work_h = work_bottom - USH_UWM_TOP_CLAMP_Y;
    if (work_h < 80) {
        work_h = 80;
    }

    base_w = ush_uwm_fit_dimension(USH_UWM_APP_START_W, USH_UWM_MIN_WINDOW_W, sess->screen_w - 48);
    base_h = ush_uwm_fit_dimension(USH_UWM_APP_START_H, USH_UWM_MIN_WINDOW_H, work_h - 32);
    app_gap = (sess->screen_w > 900) ? 120 : 58;

    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        int max_x = sess->screen_w - base_w;
        int max_y = work_bottom - base_h;
        int x = 56 + (i * app_gap);
        int y = 64 + (i * 46);

        if (max_x < 0) {
            max_x = 0;
        }
        if (max_y < USH_UWM_TOP_CLAMP_Y) {
            max_y = USH_UWM_TOP_CLAMP_Y;
        }

        ush_uwm_init_window(&sess->windows[i], USH_UWM_KIND_APP, titles[i], subtitles[i], ush_uwm_clampi(x, 0, max_x),
                            ush_uwm_clampi(y, USH_UWM_TOP_CLAMP_Y, max_y), base_w, base_h, accents[i], 0, 0);
    }

    ush_uwm_init_window(&sess->windows[USH_UWM_TASKBAR_INDEX], USH_UWM_KIND_TASKBAR, "TASKBAR", "", 0,
                        sess->screen_h - USH_UWM_TASKBAR_H, sess->screen_w, USH_UWM_TASKBAR_H, 0x000078D7U, 1, 0);
    if (sess->windows[USH_UWM_TASKBAR_INDEX].y < USH_UWM_TOP_CLAMP_Y) {
        sess->windows[USH_UWM_TASKBAR_INDEX].y = USH_UWM_TOP_CLAMP_Y;
    }

    start_w = ush_uwm_fit_dimension(USH_UWM_START_W, 180, sess->screen_w - 16);
    start_h = ush_uwm_fit_dimension(USH_UWM_START_H, 160, work_h - 8);
    ush_uwm_init_window(&sess->windows[USH_UWM_START_INDEX], USH_UWM_KIND_START, "START", "", 0,
                        sess->screen_h - USH_UWM_TASKBAR_H - start_h, start_w, start_h, 0x000078D7U, 1, 1);
    if (sess->windows[USH_UWM_START_INDEX].y < USH_UWM_TOP_CLAMP_Y) {
        sess->windows[USH_UWM_START_INDEX].y = USH_UWM_TOP_CLAMP_Y;
    }

    return 1;
}

int ush_uwm_start(ush_uwm_session *sess) {
    int i;
    int started = 0;

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    if (ush_uwm_boot_window(sess, USH_UWM_TASKBAR_INDEX) == 0) {
        return ush_uwm_fail(sess, "uwm: taskbar create failed");
    }

    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        if (ush_uwm_boot_window(sess, i) != 0) {
            started++;
        }
    }

    if (started == 0) {
        return ush_uwm_fail(sess, "uwm: app window create failed");
    }

    for (i = (int)USH_UWM_APP_COUNT - 1; i >= 0; i--) {
        if (sess->windows[i].alive != 0) {
            ush_uwm_set_active(sess, i);
            break;
        }
    }

    ush_uwm_refresh_taskbar(sess);
    return 1;
}

void ush_uwm_stop(ush_uwm_session *sess) {
    int i;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    for (i = 0; i < (int)USH_UWM_WINDOW_COUNT; i++) {
        ush_uwm_destroy_window(&sess->windows[i]);
    }
}

int ush_uwm_switch_to_display_tty(ush_uwm_session *sess) {
    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    if (sess->tty_before != USH_UWM_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(USH_UWM_TTY_DISPLAY);
        sess->tty_switched = 1;
    }

    return 1;
}

void ush_uwm_restore_tty(ush_uwm_session *sess) {
    if (sess == (ush_uwm_session *)0) {
        return;
    }

    if (sess->tty_switched != 0) {
        (void)cleonos_sys_tty_switch(sess->tty_before);
        sess->tty_switched = 0;
    }
}

int ush_cmd_uwm(const char *arg) {
    ush_uwm_session sess;
    int parse_result;
    int success = 0;

    parse_result = ush_uwm_parse_args(arg);
    if (parse_result == 2) {
        ush_uwm_usage();
        return 1;
    }

    if (parse_result == 0) {
        ush_uwm_usage();
        return 0;
    }

    if (ush_uwm_prepare_session(&sess) == 0) {
        ush_writeln(sess.last_error);
        return 0;
    }

    ush_uwm_drain_startup_keys();
    (void)ush_uwm_switch_to_display_tty(&sess);

    if (ush_uwm_start(&sess) == 0) {
        ush_uwm_stop(&sess);
        ush_uwm_restore_tty(&sess);
        ush_writeln(sess.last_error);
        return 0;
    }

    if (ush_uwm_loop(&sess) != 0) {
        success = 1;
    }

    ush_uwm_stop(&sess);
    ush_uwm_restore_tty(&sess);
    ush_writeln("uwm: exit");
    return success;
}
