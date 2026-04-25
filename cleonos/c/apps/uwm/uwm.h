#ifndef CLEONOS_UWM_H
#define CLEONOS_UWM_H

#include "../cmd_runtime.h"

#define USH_UWM_WINDOW_COUNT 3U
#define USH_UWM_TTY_DISPLAY 1ULL
#define USH_UWM_TITLE_DRAG_HEIGHT 18
#define USH_UWM_MOVE_STEP 12
#define USH_UWM_EVENT_BUDGET 128ULL
#define USH_UWM_STARTUP_KEY_DRAIN_MAX 256ULL
#define USH_UWM_MIN_WINDOW_W 180
#define USH_UWM_MIN_WINDOW_H 120
#define USH_UWM_TOP_CLAMP_Y 24

#define USH_UWM_KEY_LEFT 1ULL
#define USH_UWM_KEY_RIGHT 2ULL
#define USH_UWM_KEY_UP 3ULL
#define USH_UWM_KEY_DOWN 4ULL

typedef unsigned int ush_uwm_u32;

typedef struct ush_uwm_window {
    u64 id;
    int x;
    int y;
    int w;
    int h;
    ush_uwm_u32 color;
    ush_uwm_u32 *pixels;
    u64 pixel_count;
    int alive;
} ush_uwm_window;

typedef struct ush_uwm_session {
    int screen_w;
    int screen_h;
    int active_window;
    int dragging;
    int drag_window;
    int drag_offset_x;
    int drag_offset_y;
    u64 tty_before;
    int tty_switched;
    ush_uwm_window windows[USH_UWM_WINDOW_COUNT];
} ush_uwm_session;

int ush_uwm_window_index_valid(int index);
int ush_uwm_clampi(int value, int min_value, int max_value);
int ush_uwm_u64_as_i32(u64 raw);
void ush_uwm_drain_startup_keys(void);

void ush_uwm_render_content(ush_uwm_window *win);
int ush_uwm_alloc_pixels(ush_uwm_window *win);
int ush_uwm_create_window(ush_uwm_window *win);
int ush_uwm_present_window(const ush_uwm_window *win);
void ush_uwm_destroy_window(ush_uwm_window *win);
int ush_uwm_window_move_clamped(ush_uwm_session *sess, int index, int target_x, int target_y);
void ush_uwm_set_active(ush_uwm_session *sess, int index);
void ush_uwm_focus_next(ush_uwm_session *sess);

void ush_uwm_handle_event(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event, int *running);
int ush_uwm_loop(ush_uwm_session *sess);

int ush_uwm_prepare_session(ush_uwm_session *sess);
int ush_uwm_start(ush_uwm_session *sess);
void ush_uwm_stop(ush_uwm_session *sess);
int ush_uwm_switch_to_display_tty(ush_uwm_session *sess);
void ush_uwm_restore_tty(ush_uwm_session *sess);
int ush_cmd_uwm(const char *arg);

#endif
