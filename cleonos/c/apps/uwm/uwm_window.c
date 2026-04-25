#include "uwm.h"

static void ush_uwm_fill_rect(ush_uwm_window *win, int x, int y, int w, int h, ush_uwm_u32 color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (win == (ush_uwm_window *)0 || win->pixels == (ush_uwm_u32 *)0 || win->w <= 0 || win->h <= 0 || w <= 0 ||
        h <= 0) {
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
    if (right > win->w) {
        right = win->w;
    }
    if (bottom > win->h) {
        bottom = win->h;
    }

    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        u64 base = (u64)(unsigned int)row * (u64)(unsigned int)win->w;
        int col;

        for (col = left; col < right; col++) {
            u64 idx = base + (u64)(unsigned int)col;
            if (idx >= win->pixel_count) {
                return;
            }
            win->pixels[idx] = color;
        }
    }
}

void ush_uwm_render_content(ush_uwm_window *win) {
    int y;

    if (win == (ush_uwm_window *)0 || win->pixels == (ush_uwm_u32 *)0) {
        return;
    }

    ush_uwm_fill_rect(win, 0, 0, win->w, win->h, win->color);
    ush_uwm_fill_rect(win, 0, 0, win->w, 18, 0x001A1F2BUL);
    ush_uwm_fill_rect(win, 8, 5, 8, 8, 0x00FFD166UL);
    ush_uwm_fill_rect(win, win->w - 18, 5, 10, 8, 0x00E76F51UL);

    for (y = 30; y < win->h; y += 22) {
        ush_uwm_fill_rect(win, 10, y, win->w - 20, 1, 0x003F4D62UL);
    }
}

int ush_uwm_alloc_pixels(ush_uwm_window *win) {
    u64 count;
    u64 bytes;

    if (win == (ush_uwm_window *)0 || win->w <= 0 || win->h <= 0) {
        return 0;
    }

    count = (u64)(unsigned int)win->w * (u64)(unsigned int)win->h;
    if (count == 0ULL || count > (((u64)-1) / 4ULL)) {
        return 0;
    }

    bytes = count * 4ULL;
    win->pixels = (ush_uwm_u32 *)malloc((size_t)bytes);
    if (win->pixels == (ush_uwm_u32 *)0) {
        return 0;
    }

    win->pixel_count = count;
    ush_zero(win->pixels, bytes);
    return 1;
}

int ush_uwm_create_window(ush_uwm_window *win) {
    cleonos_wm_create_req req;

    if (win == (ush_uwm_window *)0) {
        return 0;
    }

    req.x = (u64)(i64)win->x;
    req.y = (u64)(i64)win->y;
    req.width = (u64)(unsigned int)win->w;
    req.height = (u64)(unsigned int)win->h;
    req.flags = 0ULL;
    win->id = cleonos_sys_wm_create(&req);
    if (win->id == 0ULL) {
        return 0;
    }

    win->alive = 1;
    return 1;
}

int ush_uwm_present_window(const ush_uwm_window *win) {
    cleonos_wm_present_req req;

    if (win == (const ush_uwm_window *)0 || win->alive == 0 || win->id == 0ULL ||
        win->pixels == (ush_uwm_u32 *)0) {
        return 0;
    }

    req.window_id = win->id;
    req.pixels_ptr = (u64)(usize)win->pixels;
    req.src_width = (u64)(unsigned int)win->w;
    req.src_height = (u64)(unsigned int)win->h;
    req.src_pitch_bytes = (u64)(unsigned int)win->w * 4ULL;
    return (cleonos_sys_wm_present(&req) != 0ULL) ? 1 : 0;
}

void ush_uwm_destroy_window(ush_uwm_window *win) {
    if (win == (ush_uwm_window *)0) {
        return;
    }

    if (win->id != 0ULL) {
        (void)cleonos_sys_wm_destroy(win->id);
    }

    if (win->pixels != (ush_uwm_u32 *)0) {
        free(win->pixels);
    }

    win->id = 0ULL;
    win->pixels = (ush_uwm_u32 *)0;
    win->pixel_count = 0ULL;
    win->alive = 0;
}

int ush_uwm_window_move_clamped(ush_uwm_session *sess, int index, int target_x, int target_y) {
    ush_uwm_window *win;
    cleonos_wm_move_req req;
    int max_x;
    int max_y;
    int new_x;
    int new_y;

    if (sess == (ush_uwm_session *)0 || ush_uwm_window_index_valid(index) == 0) {
        return 0;
    }

    win = &sess->windows[index];
    if (win->alive == 0 || win->id == 0ULL) {
        return 0;
    }

    max_x = sess->screen_w - win->w;
    max_y = sess->screen_h - win->h;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }

    new_x = ush_uwm_clampi(target_x, 0, max_x);
    new_y = ush_uwm_clampi(target_y, USH_UWM_TOP_CLAMP_Y, max_y);

    req.window_id = win->id;
    req.x = (u64)(i64)new_x;
    req.y = (u64)(i64)new_y;
    if (cleonos_sys_wm_move(&req) == 0ULL) {
        return 0;
    }

    win->x = new_x;
    win->y = new_y;
    return 1;
}

void ush_uwm_set_active(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;

    if (sess == (ush_uwm_session *)0 || ush_uwm_window_index_valid(index) == 0) {
        return;
    }

    win = &sess->windows[index];
    if (win->alive == 0 || win->id == 0ULL) {
        return;
    }

    if (cleonos_sys_wm_set_focus(win->id) != 0ULL) {
        sess->active_window = index;
    }
}

void ush_uwm_focus_next(ush_uwm_session *sess) {
    int start;
    int i;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    if (ush_uwm_window_index_valid(sess->active_window) != 0) {
        start = sess->active_window;
    } else {
        start = 0;
    }

    for (i = 1; i <= (int)USH_UWM_WINDOW_COUNT; i++) {
        int idx = (start + i) % (int)USH_UWM_WINDOW_COUNT;
        if (sess->windows[idx].alive != 0) {
            ush_uwm_set_active(sess, idx);
            return;
        }
    }
}
