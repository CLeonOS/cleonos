#ifndef CLEONOS_UWM_H
#define CLEONOS_UWM_H

#include "../cmd_runtime.h"

#define USH_UWM_APP_COUNT 3U
#define USH_UWM_TERMINAL_INDEX 1
#define USH_UWM_TASKMGR_INDEX 2
#define USH_UWM_TASKBAR_INDEX 3
#define USH_UWM_START_INDEX 4
#define USH_UWM_WINDOW_COUNT 5U
#define USH_UWM_TTY_DISPLAY 1ULL

#define USH_UWM_TOP_CLAMP_Y 24
#define USH_UWM_TITLE_H 32
#define USH_UWM_TASKBAR_H 48
#define USH_UWM_START_W 320
#define USH_UWM_START_H 360
#define USH_UWM_APP_START_W 360
#define USH_UWM_APP_START_H 240
#define USH_UWM_MIN_WINDOW_W 220
#define USH_UWM_MIN_WINDOW_H 150
#define USH_UWM_MOVE_STEP 16
#define USH_UWM_RESIZE_GRIP 18
#define USH_UWM_CONTROL_W 46
#define USH_UWM_EVENT_BUDGET 128ULL
#define USH_UWM_STARTUP_KEY_DRAIN_MAX 256ULL
#define USH_UWM_IDLE_SPINS 32

#define USH_UWM_TASKBAR_START_W 48
#define USH_UWM_TASKBAR_SEARCH_W 220
#define USH_UWM_TASKBAR_BUTTON_W 132
#define USH_UWM_TASKBAR_BUTTON_GAP 6
#define USH_UWM_TASKBAR_DYNAMIC_MAX 32ULL

#define USH_UWM_KEY_LEFT 1ULL
#define USH_UWM_KEY_RIGHT 2ULL
#define USH_UWM_KEY_UP 3ULL
#define USH_UWM_KEY_DOWN 4ULL

#define USH_UWM_WM_FLAG_TOPMOST 0x1ULL

typedef unsigned int ush_uwm_u32;

typedef enum ush_uwm_window_kind {
    USH_UWM_KIND_APP = 0,
    USH_UWM_KIND_TASKBAR = 1,
    USH_UWM_KIND_START = 2
} ush_uwm_window_kind;

typedef struct ush_uwm_window {
    u64 id;
    int x;
    int y;
    int w;
    int h;
    ush_uwm_u32 *pixels;
    u64 pixel_count;
    int alive;
    int minimized;
    int closed;
    int topmost;
    int maximized;
    int restore_x;
    int restore_y;
    int restore_w;
    int restore_h;
    int dirty;
    ush_uwm_window_kind kind;
    ush_uwm_u32 accent;
    char title[32];
    char subtitle[64];
} ush_uwm_window;

typedef struct ush_uwm_session {
    int screen_w;
    int screen_h;
    int active_window;
    int dragging;
    int drag_window;
    int drag_offset_x;
    int drag_offset_y;
    int resizing;
    int resize_window;
    int resize_start_x;
    int resize_start_y;
    int resize_start_w;
    int resize_start_h;
    int resize_pending_w;
    int resize_pending_h;
    int start_open;
    u64 mouse_packet_seen;
    u64 app_registry_last_tick;
    u64 tty_before;
    int tty_switched;
    char last_error[96];
    u64 app_pids[USH_UWM_APP_COUNT];
    u64 app_states[USH_UWM_APP_COUNT];
    ush_uwm_window windows[USH_UWM_WINDOW_COUNT];
} ush_uwm_session;

int ush_uwm_window_index_valid(int index);
int ush_uwm_app_index_valid(int index);
int ush_uwm_clampi(int value, int min_value, int max_value);
int ush_uwm_u64_as_i32(u64 raw);
void ush_uwm_drain_startup_keys(void);
int ush_uwm_app_registry_running(ush_uwm_session *sess, int index);
int ush_uwm_refresh_app_registry(ush_uwm_session *sess);
int ush_uwm_local_window_index_by_id(const ush_uwm_session *sess, u64 window_id);
int ush_uwm_taskbar_window_hidden(const ush_uwm_session *sess, u64 window_id);
void ush_uwm_taskbar_window_label(const ush_uwm_session *sess, const cleonos_wm_snapshot *snap, char *out_label,
                                  u64 out_size);

int ush_uwm_alloc_pixels(ush_uwm_window *win);
int ush_uwm_replace_pixels(ush_uwm_window *win, int width, int height);
void ush_uwm_render_window(ush_uwm_session *sess, int index);
void ush_uwm_refresh_window(ush_uwm_session *sess, int index);
int ush_uwm_create_window(ush_uwm_window *win);
int ush_uwm_present_window(const ush_uwm_window *win);
void ush_uwm_destroy_kernel_window(ush_uwm_window *win);
void ush_uwm_destroy_window(ush_uwm_window *win);
int ush_uwm_window_move_clamped(ush_uwm_session *sess, int index, int target_x, int target_y);
int ush_uwm_window_resize(ush_uwm_session *sess, int index, int target_w, int target_h);
void ush_uwm_set_active(ush_uwm_session *sess, int index);
void ush_uwm_focus_next(ush_uwm_session *sess);
void ush_uwm_minimize_window(ush_uwm_session *sess, int index);
void ush_uwm_close_window(ush_uwm_session *sess, int index);
void ush_uwm_restore_window(ush_uwm_session *sess, int index);
void ush_uwm_toggle_topmost(ush_uwm_session *sess, int index);
void ush_uwm_toggle_maximize(ush_uwm_session *sess, int index);
void ush_uwm_toggle_start(ush_uwm_session *sess);
void ush_uwm_close_start(ush_uwm_session *sess);
void ush_uwm_refresh_taskbar(ush_uwm_session *sess);

void ush_uwm_handle_event(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event, int *running);
int ush_uwm_loop(ush_uwm_session *sess);

int ush_uwm_prepare_session(ush_uwm_session *sess);
int ush_uwm_start(ush_uwm_session *sess);
void ush_uwm_stop(ush_uwm_session *sess);
int ush_uwm_switch_to_display_tty(ush_uwm_session *sess);
void ush_uwm_restore_tty(ush_uwm_session *sess);
int ush_cmd_uwm(const char *arg);

#endif
