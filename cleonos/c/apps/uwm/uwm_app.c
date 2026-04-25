#include "uwm.h"

static void ush_uwm_usage(void) {
    ush_writeln("usage: uwm");
    ush_writeln("wm mode: kernel compositor + user window manager");
    ush_writeln("keys: q quit, tab focus next, 1/2/3 focus, wasd/arrow move");
    ush_writeln("mouse: drag focused window by title bar");
}

static int ush_uwm_parse_args(const char *arg) {
    char first[USH_PATH_MAX];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    return 0;
}

int ush_uwm_prepare_session(ush_uwm_session *sess) {
    cleonos_fb_info fb;
    int i;
    int base_w;
    int base_h;
    const int x_offsets[USH_UWM_WINDOW_COUNT] = {64, 220, 380};
    const int y_offsets[USH_UWM_WINDOW_COUNT] = {80, 140, 220};
    const ush_uwm_u32 colors[USH_UWM_WINDOW_COUNT] = {0x002B3C66UL, 0x00385C7AUL, 0x004A7288UL};

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    ush_zero(sess, (u64)sizeof(*sess));
    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL) {
        return 0;
    }

    if (fb.width > 4096ULL || fb.height > 4096ULL) {
        return 0;
    }

    sess->screen_w = (int)fb.width;
    sess->screen_h = (int)fb.height;
    sess->active_window = 0;
    sess->dragging = 0;
    sess->drag_window = -1;
    sess->drag_offset_x = 0;
    sess->drag_offset_y = 0;
    sess->tty_before = cleonos_sys_tty_active();
    sess->tty_switched = 0;

    base_w = sess->screen_w / 3;
    base_h = sess->screen_h / 3;
    if (base_w < USH_UWM_MIN_WINDOW_W) {
        base_w = USH_UWM_MIN_WINDOW_W;
    }
    if (base_h < USH_UWM_MIN_WINDOW_H) {
        base_h = USH_UWM_MIN_WINDOW_H;
    }
    if (base_w > sess->screen_w - 40) {
        base_w = sess->screen_w - 40;
    }
    if (base_h > sess->screen_h - 40) {
        base_h = sess->screen_h - 40;
    }

    for (i = 0; i < (int)USH_UWM_WINDOW_COUNT; i++) {
        ush_uwm_window *win = &sess->windows[i];
        int max_x = sess->screen_w - base_w;
        int max_y = sess->screen_h - base_h;

        if (max_x < 0) {
            max_x = 0;
        }
        if (max_y < 0) {
            max_y = 0;
        }

        win->id = 0ULL;
        win->x = ush_uwm_clampi(x_offsets[i], 0, max_x);
        win->y = ush_uwm_clampi(y_offsets[i], USH_UWM_TOP_CLAMP_Y, max_y);
        win->w = base_w;
        win->h = base_h;
        win->color = colors[i];
        win->pixels = (ush_uwm_u32 *)0;
        win->pixel_count = 0ULL;
        win->alive = 0;
    }

    return 1;
}

int ush_uwm_start(ush_uwm_session *sess) {
    int i;

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    for (i = 0; i < (int)USH_UWM_WINDOW_COUNT; i++) {
        ush_uwm_window *win = &sess->windows[i];

        if (ush_uwm_alloc_pixels(win) == 0) {
            return 0;
        }

        if (ush_uwm_create_window(win) == 0) {
            return 0;
        }

        ush_uwm_render_content(win);
        if (ush_uwm_present_window(win) == 0) {
            return 0;
        }
    }

    sess->active_window = (int)USH_UWM_WINDOW_COUNT - 1;
    ush_uwm_set_active(sess, sess->active_window);
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
        ush_writeln("uwm: framebuffer unavailable");
        return 0;
    }

    ush_uwm_drain_startup_keys();
    (void)ush_uwm_switch_to_display_tty(&sess);

    if (ush_uwm_start(&sess) == 0) {
        ush_uwm_stop(&sess);
        ush_uwm_restore_tty(&sess);
        ush_writeln("uwm: kernel wm unavailable or init failed");
        return 0;
    }

    ush_writeln("uwm: kernel window framework online");
    ush_writeln("uwm: q quit | tab focus | 1/2/3 focus | wasd/arrow move");

    if (ush_uwm_loop(&sess) != 0) {
        success = 1;
    }

    ush_uwm_stop(&sess);
    ush_uwm_restore_tty(&sess);
    ush_writeln("uwm: exit");
    return success;
}
