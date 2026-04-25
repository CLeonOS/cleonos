#include "cmd_runtime.h"

#define USH_UWM_WINDOW_COUNT 3ULL
#define USH_UWM_TITLE_HEIGHT 18
#define USH_UWM_BORDER_THICKNESS 2
#define USH_UWM_WINDOW_MIN_W 120
#define USH_UWM_WINDOW_MIN_H 80
#define USH_UWM_STARTUP_KEY_DRAIN_MAX 256ULL
#define USH_UWM_BLIT_MAX_FAIL_STREAK 30ULL

#define USH_UWM_KEY_LEFT 1
#define USH_UWM_KEY_RIGHT 2
#define USH_UWM_KEY_UP 3
#define USH_UWM_KEY_DOWN 4

#define USH_UWM_COLOR_BG 0x00161D2BUL
#define USH_UWM_COLOR_PANEL 0x00263352UL
#define USH_UWM_COLOR_CURSOR 0x00FFFFFFUL
#define USH_UWM_COLOR_CURSOR_SHADOW 0x00000000UL
#define USH_UWM_COLOR_BORDER_ACTIVE 0x00FFD166UL
#define USH_UWM_COLOR_BORDER_INACTIVE 0x004A566EL
#define USH_UWM_COLOR_TITLE_ACTIVE 0x005A77B7UL
#define USH_UWM_COLOR_TITLE_INACTIVE 0x00394C78UL

typedef unsigned int ush_uwm_u32;

typedef struct ush_uwm_window {
    int x;
    int y;
    int w;
    int h;
    ush_uwm_u32 body_color;
} ush_uwm_window;

typedef struct ush_uwm_runtime {
    cleonos_fb_info fb;
    ush_uwm_u32 *canvas;
    u64 canvas_pixels;
    int screen_w;
    int screen_h;
    int mouse_x;
    int mouse_y;
    u64 mouse_buttons;
    int mouse_ready;
} ush_uwm_runtime;

static int ush_uwm_window_index_valid(int index) {
    return (index >= 0 && index < (int)USH_UWM_WINDOW_COUNT) ? 1 : 0;
}

static void ush_uwm_z_order_sanitize(int *z_order) {
    int seen[USH_UWM_WINDOW_COUNT];
    int fill_index = 0;
    u64 i;

    if (z_order == (int *)0) {
        return;
    }

    for (i = 0ULL; i < USH_UWM_WINDOW_COUNT; i++) {
        seen[i] = 0;
    }

    for (i = 0ULL; i < USH_UWM_WINDOW_COUNT; i++) {
        int value = z_order[i];

        if (ush_uwm_window_index_valid(value) != 0 && seen[(u64)value] == 0) {
            seen[(u64)value] = 1;
            continue;
        }

        while (fill_index < (int)USH_UWM_WINDOW_COUNT && seen[(u64)fill_index] != 0) {
            fill_index++;
        }

        if (fill_index >= (int)USH_UWM_WINDOW_COUNT) {
            fill_index = (int)USH_UWM_WINDOW_COUNT - 1;
        }

        z_order[i] = fill_index;
        seen[(u64)fill_index] = 1;
    }
}

static int ush_uwm_runtime_sane(const ush_uwm_runtime *rt) {
    u64 min_pixels;

    if (rt == (const ush_uwm_runtime *)0 || rt->canvas == (ush_uwm_u32 *)0) {
        return 0;
    }

    if (rt->screen_w <= 0 || rt->screen_h <= 0 || rt->screen_w > 4096 || rt->screen_h > 4096) {
        return 0;
    }

    min_pixels = (u64)(unsigned int)rt->screen_w * (u64)(unsigned int)rt->screen_h;
    if (rt->canvas_pixels < min_pixels) {
        return 0;
    }

    return 1;
}

static int ush_uwm_clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static int ush_uwm_u64_to_i32_sat(u64 value) {
    if (value > 0x7FFFFFFFULL) {
        return 0x7FFFFFFF;
    }

    return (int)value;
}

static void ush_uwm_fill_rect(ush_uwm_runtime *rt, int x, int y, int w, int h, ush_uwm_u32 color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (ush_uwm_runtime_sane(rt) == 0 || w <= 0 || h <= 0) {
        return;
    }

    left = x;
    top = y;
    right = x + w;
    bottom = y + h;

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > rt->screen_w) {
        right = rt->screen_w;
    }
    if (bottom > rt->screen_h) {
        bottom = rt->screen_h;
    }

    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        u64 offset = ((u64)(unsigned int)row * (u64)(unsigned int)rt->screen_w) + (u64)(unsigned int)left;
        u64 max_pixels = rt->canvas_pixels;
        int col;

        if (offset >= max_pixels) {
            break;
        }

        for (col = left; col < right; col++) {
            if (offset >= max_pixels) {
                break;
            }
            rt->canvas[offset++] = color;
        }
    }
}

static void ush_uwm_draw_window(ush_uwm_runtime *rt, const ush_uwm_window *win, int active) {
    ush_uwm_u32 border_color = (active != 0) ? USH_UWM_COLOR_BORDER_ACTIVE : USH_UWM_COLOR_BORDER_INACTIVE;
    ush_uwm_u32 title_color = (active != 0) ? USH_UWM_COLOR_TITLE_ACTIVE : USH_UWM_COLOR_TITLE_INACTIVE;
    int title_h;

    if (rt == (ush_uwm_runtime *)0 || win == (const ush_uwm_window *)0) {
        return;
    }

    title_h = USH_UWM_TITLE_HEIGHT;
    if (title_h > win->h) {
        title_h = win->h;
    }

    ush_uwm_fill_rect(rt, win->x, win->y, win->w, win->h, border_color);
    ush_uwm_fill_rect(rt, win->x + USH_UWM_BORDER_THICKNESS, win->y + USH_UWM_BORDER_THICKNESS,
                      win->w - (USH_UWM_BORDER_THICKNESS * 2), title_h - USH_UWM_BORDER_THICKNESS, title_color);
    ush_uwm_fill_rect(rt, win->x + USH_UWM_BORDER_THICKNESS, win->y + title_h, win->w - (USH_UWM_BORDER_THICKNESS * 2),
                      win->h - title_h - USH_UWM_BORDER_THICKNESS, win->body_color);

    ush_uwm_fill_rect(rt, win->x + win->w - 16, win->y + 4, 10, 10, 0x00E76F51UL);
}

static void ush_uwm_draw_cursor(ush_uwm_runtime *rt, int x, int y) {
    int i;

    if (rt == (ush_uwm_runtime *)0) {
        return;
    }

    for (i = -6; i <= 6; i++) {
        ush_uwm_fill_rect(rt, x + i, y, 1, 1, (i == 0) ? USH_UWM_COLOR_CURSOR : USH_UWM_COLOR_CURSOR_SHADOW);
        ush_uwm_fill_rect(rt, x, y + i, 1, 1, (i == 0) ? USH_UWM_COLOR_CURSOR : USH_UWM_COLOR_CURSOR_SHADOW);
    }

    ush_uwm_fill_rect(rt, x - 1, y - 1, 3, 3, USH_UWM_COLOR_CURSOR);
}

static void ush_uwm_draw_background(ush_uwm_runtime *rt) {
    int y;

    if (rt == (ush_uwm_runtime *)0) {
        return;
    }

    ush_uwm_fill_rect(rt, 0, 0, rt->screen_w, rt->screen_h, USH_UWM_COLOR_BG);
    ush_uwm_fill_rect(rt, 0, 0, rt->screen_w, 30, USH_UWM_COLOR_PANEL);

    for (y = 60; y < rt->screen_h; y += 40) {
        ush_uwm_fill_rect(rt, 0, y, rt->screen_w, 1, 0x001E283BUL);
    }
}

static int ush_uwm_window_hit_title(const ush_uwm_window *win, int x, int y) {
    int title_h;

    if (win == (const ush_uwm_window *)0) {
        return 0;
    }

    title_h = USH_UWM_TITLE_HEIGHT;
    if (title_h > win->h) {
        title_h = win->h;
    }

    if (x < win->x || y < win->y) {
        return 0;
    }

    if (x >= win->x + win->w || y >= win->y + title_h) {
        return 0;
    }

    return 1;
}

static void ush_uwm_bring_to_front(int *z_order, u64 count, int window_index) {
    u64 i;
    u64 pos = count;

    if (z_order == (int *)0 || count == 0ULL || ush_uwm_window_index_valid(window_index) == 0) {
        return;
    }

    for (i = 0ULL; i < count; i++) {
        if (z_order[i] == window_index) {
            pos = i;
            break;
        }
    }

    if (pos >= count || pos + 1ULL >= count) {
        return;
    }

    for (i = pos; i + 1ULL < count; i++) {
        z_order[i] = z_order[i + 1ULL];
    }
    z_order[count - 1ULL] = window_index;
}

static int ush_uwm_cycle_focus(const int *z_order, u64 count, int active_window) {
    u64 i;
    u64 pos = 0ULL;

    if (z_order == (const int *)0 || count == 0ULL) {
        return 0;
    }

    if (ush_uwm_window_index_valid(active_window) == 0) {
        return z_order[count - 1ULL];
    }

    for (i = 0ULL; i < count; i++) {
        if (z_order[i] == active_window) {
            pos = i;
            break;
        }
    }

    return z_order[(pos + 1ULL) % count];
}

static int ush_uwm_poll_mouse(ush_uwm_runtime *rt) {
    cleonos_mouse_state ms;
    int max_x;
    int max_y;

    if (rt == (ush_uwm_runtime *)0) {
        return 0;
    }

    ush_zero(&ms, (u64)sizeof(ms));
    if (cleonos_sys_mouse_state(&ms) == 0ULL) {
        return 0;
    }

    rt->mouse_buttons = ms.buttons;
    rt->mouse_ready = (ms.ready != 0ULL) ? 1 : 0;

    if (rt->screen_w <= 0 || rt->screen_h <= 0) {
        return rt->mouse_ready;
    }

    max_x = rt->screen_w - 1;
    max_y = rt->screen_h - 1;

    rt->mouse_x = ush_uwm_clampi(ush_uwm_u64_to_i32_sat(ms.x), 0, max_x);
    rt->mouse_y = ush_uwm_clampi(ush_uwm_u64_to_i32_sat(ms.y), 0, max_y);
    return rt->mouse_ready;
}

static int ush_uwm_present(ush_uwm_runtime *rt) {
    cleonos_fb_blit_req req;

    if (ush_uwm_runtime_sane(rt) == 0) {
        return 0;
    }

    req.pixels_ptr = (u64)(usize)rt->canvas;
    req.src_width = (u64)(unsigned int)rt->screen_w;
    req.src_height = (u64)(unsigned int)rt->screen_h;
    req.src_pitch_bytes = (u64)(unsigned int)rt->screen_w * 4ULL;
    req.dst_x = 0ULL;
    req.dst_y = 0ULL;
    req.scale = 1ULL;
    return (cleonos_sys_fb_blit(&req) != 0ULL) ? 1 : 0;
}

static u64 ush_uwm_drain_startup_keys(void) {
    u64 drained = 0ULL;

    while (drained < USH_UWM_STARTUP_KEY_DRAIN_MAX) {
        u64 key = cleonos_sys_kbd_get_char();
        if (key == (u64)-1) {
            break;
        }
        drained++;
    }

    return drained;
}

/* return: 0 fail, 1 ok, 2 help */
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

static void ush_uwm_usage(void) {
    ush_writeln("usage: uwm");
    ush_writeln("keys: q quit, tab cycle focus, wasd move active window");
    ush_writeln("mouse: drag window by title bar");
}

static int ush_cmd_uwm(const char *arg) {
    ush_uwm_runtime rt;
    ush_uwm_window windows[USH_UWM_WINDOW_COUNT];
    int z_order[USH_UWM_WINDOW_COUNT];
    int active_window = 0;
    int dragging = 0;
    int drag_window = 0;
    int drag_offset_x = 0;
    int drag_offset_y = 0;
    u64 prev_buttons = 0ULL;
    u64 blit_fail_streak = 0ULL;
    int running = 1;
    int parse_result;
    u64 drained_keys;

    parse_result = ush_uwm_parse_args(arg);
    if (parse_result == 2) {
        ush_uwm_usage();
        return 1;
    }

    if (parse_result == 0) {
        ush_uwm_usage();
        return 0;
    }

    ush_zero(&rt, (u64)sizeof(rt));

    if (cleonos_sys_fb_info(&rt.fb) == 0ULL || rt.fb.width == 0ULL || rt.fb.height == 0ULL || rt.fb.bpp != 32ULL) {
        ush_writeln("uwm: framebuffer unavailable");
        return 0;
    }

    if (rt.fb.width > 4096ULL || rt.fb.height > 4096ULL) {
        ush_writeln("uwm: framebuffer too large");
        return 0;
    }

    rt.screen_w = (int)rt.fb.width;
    rt.screen_h = (int)rt.fb.height;
    rt.canvas_pixels = rt.fb.width * rt.fb.height;
    rt.canvas = (ush_uwm_u32 *)malloc((size_t)(rt.canvas_pixels * 4ULL));
    if (rt.canvas == (ush_uwm_u32 *)0) {
        ush_writeln("uwm: framebuffer buffer alloc failed");
        return 0;
    }

    windows[0].x = 50;
    windows[0].y = 70;
    windows[0].w = rt.screen_w / 3;
    windows[0].h = rt.screen_h / 3;
    windows[0].body_color = 0x002C3E6BUL;

    windows[1].x = 180;
    windows[1].y = 140;
    windows[1].w = rt.screen_w / 3;
    windows[1].h = rt.screen_h / 3;
    windows[1].body_color = 0x003F5B7EUL;

    windows[2].x = 310;
    windows[2].y = 210;
    windows[2].w = rt.screen_w / 3;
    windows[2].h = rt.screen_h / 3;
    windows[2].body_color = 0x00506F89UL;

    z_order[0] = 0;
    z_order[1] = 1;
    z_order[2] = 2;
    active_window = 2;

    {
        u64 i;
        for (i = 0ULL; i < USH_UWM_WINDOW_COUNT; i++) {
            int max_w = rt.screen_w - 20;
            int max_h = rt.screen_h - 40;
            if (max_w < USH_UWM_WINDOW_MIN_W) {
                max_w = USH_UWM_WINDOW_MIN_W;
            }
            if (max_h < USH_UWM_WINDOW_MIN_H) {
                max_h = USH_UWM_WINDOW_MIN_H;
            }

            if (windows[i].w < USH_UWM_WINDOW_MIN_W) {
                windows[i].w = USH_UWM_WINDOW_MIN_W;
            }
            if (windows[i].h < USH_UWM_WINDOW_MIN_H) {
                windows[i].h = USH_UWM_WINDOW_MIN_H;
            }
            if (windows[i].w > max_w) {
                windows[i].w = max_w;
            }
            if (windows[i].h > max_h) {
                windows[i].h = max_h;
            }
        }
    }

    rt.mouse_x = rt.screen_w / 2;
    rt.mouse_y = rt.screen_h / 2;
    rt.mouse_buttons = 0ULL;
    rt.mouse_ready = 0;

    ush_writeln("uwm: enter user window manager");
    ush_writeln("uwm: q quit, tab focus, wasd move, drag title bar with mouse");

    (void)cleonos_sys_fb_clear(USH_UWM_COLOR_BG);
    drained_keys = ush_uwm_drain_startup_keys();
    if (drained_keys > 0ULL) {
        ush_writeln("uwm: stale keyboard input discarded");
    }

    while (running != 0) {
        u64 key;
        int left_down;
        int left_press;
        int left_release;

        if (ush_uwm_runtime_sane(&rt) == 0) {
            free(rt.canvas);
            ush_writeln("uwm: runtime state corrupted");
            return 0;
        }

        ush_uwm_z_order_sanitize(z_order);
        if (ush_uwm_window_index_valid(active_window) == 0) {
            active_window = z_order[USH_UWM_WINDOW_COUNT - 1ULL];
        }
        if (ush_uwm_window_index_valid(drag_window) == 0) {
            dragging = 0;
            drag_window = active_window;
        }

        (void)ush_uwm_poll_mouse(&rt);

        for (;;) {
            key = cleonos_sys_kbd_get_char();
            if (key == (u64)-1) {
                break;
            }

            if (key == (u64)'q' || key == (u64)'Q') {
                running = 0;
                break;
            }

            if (key == 27ULL) {
                continue;
            }

            if (key == (u64)'\t') {
                active_window = ush_uwm_cycle_focus(z_order, USH_UWM_WINDOW_COUNT, active_window);
                if (ush_uwm_window_index_valid(active_window) != 0) {
                    ush_uwm_bring_to_front(z_order, USH_UWM_WINDOW_COUNT, active_window);
                }
                continue;
            }

            if (ush_uwm_window_index_valid(active_window) == 0) {
                continue;
            }

            if (key == (u64)'a' || key == (u64)'A') {
                windows[active_window].x -= 8;
            } else if (key == (u64)'d' || key == (u64)'D') {
                windows[active_window].x += 8;
            } else if (key == (u64)'w' || key == (u64)'W') {
                windows[active_window].y -= 8;
            } else if (key == (u64)'s' || key == (u64)'S') {
                windows[active_window].y += 8;
            } else if (key == USH_UWM_KEY_LEFT) {
                rt.mouse_x -= 10;
            } else if (key == USH_UWM_KEY_RIGHT) {
                rt.mouse_x += 10;
            } else if (key == USH_UWM_KEY_UP) {
                rt.mouse_y -= 10;
            } else if (key == USH_UWM_KEY_DOWN) {
                rt.mouse_y += 10;
            }
        }

        if (running == 0) {
            break;
        }

        left_down = ((rt.mouse_buttons & 0x01ULL) != 0ULL) ? 1 : 0;
        left_press = (left_down != 0 && (prev_buttons & 0x01ULL) == 0ULL) ? 1 : 0;
        left_release = (left_down == 0 && (prev_buttons & 0x01ULL) != 0ULL) ? 1 : 0;

        if (left_press != 0) {
            int hit = -1;
            i64 z;

            for (z = (i64)USH_UWM_WINDOW_COUNT - 1LL; z >= 0LL; z--) {
                int win_index = z_order[(u64)z];
                if (ush_uwm_window_index_valid(win_index) == 0) {
                    continue;
                }
                if (ush_uwm_window_hit_title(&windows[win_index], rt.mouse_x, rt.mouse_y) != 0) {
                    hit = win_index;
                    break;
                }
            }

            if (hit >= 0) {
                active_window = hit;
                ush_uwm_bring_to_front(z_order, USH_UWM_WINDOW_COUNT, hit);
                dragging = 1;
                drag_window = hit;
                drag_offset_x = rt.mouse_x - windows[hit].x;
                drag_offset_y = rt.mouse_y - windows[hit].y;
            }
        }

        if (left_release != 0) {
            dragging = 0;
        }

        if (dragging != 0 && left_down != 0 && ush_uwm_window_index_valid(drag_window) != 0) {
            int new_x = rt.mouse_x - drag_offset_x;
            int new_y = rt.mouse_y - drag_offset_y;
            int max_x = rt.screen_w - windows[drag_window].w;
            int max_y = rt.screen_h - windows[drag_window].h;
            int min_y = (max_y >= 30) ? 30 : 0;

            if (max_x < 0) {
                max_x = 0;
            }
            if (max_y < 0) {
                max_y = 0;
            }

            windows[drag_window].x = ush_uwm_clampi(new_x, 0, max_x);
            windows[drag_window].y = ush_uwm_clampi(new_y, min_y, max_y);
        }

        if (ush_uwm_window_index_valid(active_window) != 0) {
            int max_x = rt.screen_w - windows[active_window].w;
            int max_y = rt.screen_h - windows[active_window].h;
            int min_y = (max_y >= 30) ? 30 : 0;

            if (max_x < 0) {
                max_x = 0;
            }
            if (max_y < 0) {
                max_y = 0;
            }

            windows[active_window].x = ush_uwm_clampi(windows[active_window].x, 0, max_x);
            windows[active_window].y = ush_uwm_clampi(windows[active_window].y, min_y, max_y);
        }
        rt.mouse_x = ush_uwm_clampi(rt.mouse_x, 0, rt.screen_w - 1);
        rt.mouse_y = ush_uwm_clampi(rt.mouse_y, 0, rt.screen_h - 1);

        ush_uwm_draw_background(&rt);
        {
            u64 i;
            for (i = 0ULL; i < USH_UWM_WINDOW_COUNT; i++) {
                int win_index = z_order[i];
                if (ush_uwm_window_index_valid(win_index) == 0) {
                    continue;
                }
                int is_active = (win_index == active_window) ? 1 : 0;
                ush_uwm_draw_window(&rt, &windows[win_index], is_active);
            }
        }
        ush_uwm_draw_cursor(&rt, rt.mouse_x, rt.mouse_y);

        if (ush_uwm_present(&rt) == 0) {
            blit_fail_streak++;
            if (blit_fail_streak == 1ULL) {
                ush_writeln("uwm: framebuffer blit failed, retrying");
            }

            if (blit_fail_streak >= USH_UWM_BLIT_MAX_FAIL_STREAK) {
                free(rt.canvas);
                ush_writeln("uwm: framebuffer blit failed too many times");
                return 0;
            }

            (void)cleonos_sys_sleep_ticks(1ULL);
            continue;
        }

        blit_fail_streak = 0ULL;

        prev_buttons = rt.mouse_buttons;
        (void)cleonos_sys_sleep_ticks(1ULL);
    }

    (void)cleonos_sys_fb_clear(0x00000000ULL);
    free(rt.canvas);
    ush_writeln("uwm: exit");
    return 1;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    char arg_buf[USH_ARG_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    (void)envp;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_zero(arg_buf, (u64)sizeof(arg_buf));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "uwm") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    if (has_context == 0 && argc > 1 && argv != (char **)0 && argv[1] != (char *)0) {
        ush_copy(arg_buf, (u64)sizeof(arg_buf), argv[1]);
        arg = arg_buf;
    }

    success = ush_cmd_uwm(arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
