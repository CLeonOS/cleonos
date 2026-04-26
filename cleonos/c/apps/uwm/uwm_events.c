#include "uwm.h"

#define USH_UWM_FILE_EXPLORER_PATH "/shell/uwm/file_explorer.elf"
#define USH_UWM_TASKMGR_PATH "/shell/uwm/taskmgr.elf"
#define USH_UWM_TERMINAL_PATH "/shell/uwm/terminal.elf"

static const char *ush_uwm_app_path(int index) {
    if (index == 0) {
        return USH_UWM_FILE_EXPLORER_PATH;
    }
    if (index == USH_UWM_TERMINAL_INDEX) {
        return USH_UWM_TERMINAL_PATH;
    }
    if (index == USH_UWM_TASKMGR_INDEX) {
        return USH_UWM_TASKMGR_PATH;
    }

    return (const char *)0;
}

static int ush_uwm_focus_first_window_for_pid(u64 pid) {
    u64 count;
    u64 i;

    if (pid == 0ULL || pid == (u64)-1) {
        return 0;
    }

    count = cleonos_sys_wm_count();
    while (count > 0ULL) {
        u64 window_id = 0ULL;
        cleonos_wm_snapshot snap;

        count--;
        i = count;
        ush_zero(&snap, (u64)sizeof(snap));
        if (cleonos_sys_wm_id_at(i, &window_id) == 0ULL || window_id == 0ULL) {
            continue;
        }
        if (cleonos_sys_wm_snapshot(window_id, &snap, (u64)sizeof(snap)) == 0ULL) {
            continue;
        }
        if (snap.owner_pid == pid) {
            return (cleonos_sys_wm_set_focus(window_id) != 0ULL) ? 1 : 0;
        }
    }

    return 0;
}

static void ush_uwm_launch_or_restore_app(ush_uwm_session *sess, int index) {
    const char *path;
    u64 pid;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    if (ush_uwm_app_registry_running(sess, index) != 0) {
        (void)ush_uwm_focus_first_window_for_pid(sess->app_pids[index]);
        ush_uwm_refresh_taskbar(sess);
        return;
    }

    path = ush_uwm_app_path(index);
    if (path == (const char *)0) {
        ush_uwm_restore_window(sess, index);
        return;
    }

    pid = cleonos_sys_spawn_pathv(path, "", "LAUNCHED_BY=uwm");
    if (pid != 0ULL && pid != (u64)-1) {
        sess->app_pids[index] = pid;
        sess->app_states[index] = CLEONOS_PROC_STATE_PENDING;
    }
    ush_uwm_refresh_taskbar(sess);
}

static int ush_uwm_hit_close(const ush_uwm_window *win, int x, int y) {
    return (win != (const ush_uwm_window *)0 && x >= win->w - USH_UWM_CONTROL_W && y >= 0 && y < USH_UWM_TITLE_H) ? 1
                                                                                                                  : 0;
}

static int ush_uwm_hit_minimize(const ush_uwm_window *win, int x, int y) {
    return (win != (const ush_uwm_window *)0 && x >= win->w - (USH_UWM_CONTROL_W * 2) &&
            x < win->w - USH_UWM_CONTROL_W && y >= 0 && y < USH_UWM_TITLE_H)
               ? 1
               : 0;
}

static int ush_uwm_hit_topmost(const ush_uwm_window *win, int x, int y) {
    return (win != (const ush_uwm_window *)0 && x >= win->w - (USH_UWM_CONTROL_W * 3) &&
            x < win->w - (USH_UWM_CONTROL_W * 2) && y >= 0 && y < USH_UWM_TITLE_H)
               ? 1
               : 0;
}

static int ush_uwm_hit_resize(const ush_uwm_window *win, int x, int y) {
    return (win != (const ush_uwm_window *)0 && x >= win->w - USH_UWM_RESIZE_GRIP && y >= win->h - USH_UWM_RESIZE_GRIP)
               ? 1
               : 0;
}

static int ush_uwm_point_in_window(const ush_uwm_window *win, int x, int y) {
    if (win == (const ush_uwm_window *)0 || win->alive == 0 || win->closed != 0 || win->id == 0ULL) {
        return 0;
    }

    return (x >= win->x && y >= win->y && x < win->x + win->w && y < win->y + win->h) ? 1 : 0;
}

static int ush_uwm_hit_session_window_at(const ush_uwm_session *sess, int global_x, int global_y) {
    int i;

    if (sess == (const ush_uwm_session *)0) {
        return -1;
    }

    if (ush_uwm_point_in_window(&sess->windows[USH_UWM_START_INDEX], global_x, global_y) != 0) {
        return USH_UWM_START_INDEX;
    }

    for (i = (int)USH_UWM_APP_COUNT - 1; i >= 0; i--) {
        if (sess->windows[i].topmost != 0 && ush_uwm_point_in_window(&sess->windows[i], global_x, global_y) != 0) {
            return i;
        }
    }

    if (ush_uwm_app_index_valid(sess->active_window) != 0 &&
        ush_uwm_point_in_window(&sess->windows[sess->active_window], global_x, global_y) != 0) {
        return sess->active_window;
    }

    for (i = (int)USH_UWM_APP_COUNT - 1; i >= 0; i--) {
        if (ush_uwm_point_in_window(&sess->windows[i], global_x, global_y) != 0) {
            return i;
        }
    }

    if (ush_uwm_point_in_window(&sess->windows[USH_UWM_TASKBAR_INDEX], global_x, global_y) != 0) {
        return USH_UWM_TASKBAR_INDEX;
    }

    return -1;
}

static void ush_uwm_repair_mouse_target(ush_uwm_session *sess, int *window_index, int *local_x, int *local_y) {
    cleonos_mouse_state mouse;
    int hit;

    if (sess == (ush_uwm_session *)0 || window_index == (int *)0 || local_x == (int *)0 || local_y == (int *)0) {
        return;
    }

    ush_zero(&mouse, (u64)sizeof(mouse));
    if (cleonos_sys_mouse_state(&mouse) == 0ULL || mouse.ready == 0ULL) {
        return;
    }

    hit = ush_uwm_hit_session_window_at(sess, ush_uwm_u64_as_i32(mouse.x), ush_uwm_u64_as_i32(mouse.y));
    if (ush_uwm_window_index_valid(hit) == 0) {
        return;
    }

    *window_index = hit;
    *local_x = ush_uwm_u64_as_i32(mouse.x) - sess->windows[hit].x;
    *local_y = ush_uwm_u64_as_i32(mouse.y) - sess->windows[hit].y;
}

static int ush_uwm_taskbar_app_x(const ush_uwm_window *taskbar) {
    int search_w = USH_UWM_TASKBAR_SEARCH_W;

    if (taskbar == (const ush_uwm_window *)0) {
        return USH_UWM_TASKBAR_START_W + 8;
    }

    if (taskbar->w < 720) {
        search_w = 128;
    }
    if (taskbar->w < 520) {
        search_w = 0;
    }

    return USH_UWM_TASKBAR_START_W + 8 + ((search_w > 0) ? (search_w + 10) : 0);
}

static void ush_uwm_finish_resize(ush_uwm_session *sess) {
    int index;

    if (sess == (ush_uwm_session *)0 || sess->resizing == 0) {
        return;
    }

    index = sess->resize_window;
    sess->resizing = 0;
    sess->resize_window = -1;
    if (ush_uwm_app_index_valid(index) != 0) {
        (void)ush_uwm_window_resize(sess, index, sess->resize_pending_w, sess->resize_pending_h);
    }
}

static void ush_uwm_handle_key_event(ush_uwm_session *sess, u64 key, int *running) {
    int idx;
    int dx = 0;
    int dy = 0;

    if (sess == (ush_uwm_session *)0 || running == (int *)0) {
        return;
    }

    if (key == (u64)'	') {
        ush_uwm_focus_next(sess);
        return;
    }

    idx = sess->active_window;
    if (key == (u64)'q' || key == (u64)'Q') {
        *running = 0;
        return;
    }

    if (key == (u64)'1' || key == (u64)'2' || key == (u64)'3') {
        ush_uwm_launch_or_restore_app(sess, (int)(key - (u64)'1'));
        return;
    }

    if (ush_uwm_app_index_valid(idx) == 0 || sess->windows[idx].alive == 0) {
        return;
    }

    if (key == (u64)'m' || key == (u64)'M') {
        ush_uwm_minimize_window(sess, idx);
        return;
    }

    if (key == (u64)'x' || key == (u64)'X') {
        ush_uwm_close_window(sess, idx);
        return;
    }

    if (key == (u64)'t' || key == (u64)'T') {
        ush_uwm_toggle_topmost(sess, idx);
        return;
    }

    if (sess->windows[idx].maximized != 0) {
        return;
    }

    if (key == (u64)'+' || key == (u64)'=') {
        (void)ush_uwm_window_resize(sess, idx, sess->windows[idx].w + 32, sess->windows[idx].h + 24);
        return;
    }

    if (key == (u64)'-') {
        (void)ush_uwm_window_resize(sess, idx, sess->windows[idx].w - 32, sess->windows[idx].h - 24);
        return;
    }

    if (key == (u64)'a' || key == (u64)'A' || key == USH_UWM_KEY_LEFT) {
        dx = -USH_UWM_MOVE_STEP;
    } else if (key == (u64)'d' || key == (u64)'D' || key == USH_UWM_KEY_RIGHT) {
        dx = USH_UWM_MOVE_STEP;
    } else if (key == (u64)'w' || key == (u64)'W' || key == USH_UWM_KEY_UP) {
        dy = -USH_UWM_MOVE_STEP;
    } else if (key == (u64)'s' || key == (u64)'S' || key == USH_UWM_KEY_DOWN) {
        dy = USH_UWM_MOVE_STEP;
    }

    if (dx != 0 || dy != 0) {
        (void)ush_uwm_window_move_clamped(sess, idx, sess->windows[idx].x + dx, sess->windows[idx].y + dy);
    }
}

static void ush_uwm_handle_taskbar_click(ush_uwm_session *sess, int local_x, int local_y) {
    const ush_uwm_window *taskbar;
    int app_x;
    int i;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    taskbar = &sess->windows[USH_UWM_TASKBAR_INDEX];
    if (local_y < 0 || local_y >= taskbar->h) {
        return;
    }

    if (local_x >= 0 && local_x < USH_UWM_TASKBAR_START_W) {
        ush_uwm_toggle_start(sess);
        return;
    }

    app_x = ush_uwm_taskbar_app_x(taskbar);
    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        ush_uwm_window *app = &sess->windows[i];

        if (local_x >= app_x && local_x < app_x + USH_UWM_TASKBAR_BUTTON_W && local_y >= 5 &&
            local_y < taskbar->h - 5) {
            if (app->alive != 0 && app->minimized == 0 && sess->active_window == i) {
                ush_uwm_minimize_window(sess, i);
            } else {
                ush_uwm_launch_or_restore_app(sess, i);
            }
            return;
        }
        app_x += USH_UWM_TASKBAR_BUTTON_W + USH_UWM_TASKBAR_BUTTON_GAP;
    }
}

static void ush_uwm_handle_start_click(ush_uwm_session *sess, int local_x, int local_y) {
    int i;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    if (local_x < 52 && local_y >= sess->windows[USH_UWM_START_INDEX].h - 52) {
        ush_uwm_close_start(sess);
        return;
    }

    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        int y = 78 + (i * 44);

        if (local_x >= 66 && local_x < sess->windows[USH_UWM_START_INDEX].w - 16 && local_y >= y && local_y < y + 34) {
            ush_uwm_close_start(sess);
            ush_uwm_launch_or_restore_app(sess, i);
            return;
        }
    }
}

static void ush_uwm_handle_mouse_button(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event) {
    ush_uwm_window *win;
    u64 buttons;
    u64 changed;
    int local_x;
    int local_y;
    int left_changed;
    int left_down;

    if (sess == (ush_uwm_session *)0 || event == (const cleonos_wm_event *)0 ||
        ush_uwm_window_index_valid(window_index) == 0) {
        return;
    }

    win = &sess->windows[window_index];
    buttons = event->arg0;
    changed = event->arg1;
    local_x = ush_uwm_u64_as_i32(event->arg2);
    local_y = ush_uwm_u64_as_i32(event->arg3);
    left_changed = ((changed & 0x1ULL) != 0ULL) ? 1 : 0;
    left_down = ((buttons & 0x1ULL) != 0ULL) ? 1 : 0;

    if (left_changed != 0 && left_down != 0) {
        ush_uwm_repair_mouse_target(sess, &window_index, &local_x, &local_y);
        if (ush_uwm_window_index_valid(window_index) == 0) {
            return;
        }
        win = &sess->windows[window_index];
    }

    if (left_changed == 0) {
        return;
    }

    if (left_down == 0) {
        if (sess->dragging != 0 && sess->drag_window == window_index) {
            sess->dragging = 0;
            sess->drag_window = -1;
        }
        if (sess->resizing != 0 && sess->resize_window == window_index) {
            ush_uwm_finish_resize(sess);
        }
        return;
    }

    if (win->kind == USH_UWM_KIND_TASKBAR) {
        ush_uwm_handle_taskbar_click(sess, local_x, local_y);
        return;
    }

    if (win->kind == USH_UWM_KIND_START) {
        ush_uwm_handle_start_click(sess, local_x, local_y);
        return;
    }

    if (ush_uwm_app_index_valid(window_index) == 0) {
        return;
    }

    if (sess->start_open != 0) {
        ush_uwm_close_start(sess);
    }

    ush_uwm_set_active(sess, window_index);

    if (ush_uwm_hit_close(win, local_x, local_y) != 0) {
        ush_uwm_close_window(sess, window_index);
        return;
    }

    if (ush_uwm_hit_minimize(win, local_x, local_y) != 0) {
        ush_uwm_minimize_window(sess, window_index);
        return;
    }

    if (ush_uwm_hit_topmost(win, local_x, local_y) != 0) {
        if (window_index == USH_UWM_TERMINAL_INDEX) {
            ush_uwm_toggle_maximize(sess, window_index);
        } else {
            ush_uwm_toggle_topmost(sess, window_index);
        }
        return;
    }

    if (win->maximized == 0 && ush_uwm_hit_resize(win, local_x, local_y) != 0) {
        sess->resizing = 1;
        sess->resize_window = window_index;
        sess->mouse_packet_seen = 0ULL;
        sess->resize_start_x = win->x + local_x;
        sess->resize_start_y = win->y + local_y;
        sess->resize_start_w = win->w;
        sess->resize_start_h = win->h;
        sess->resize_pending_w = win->w;
        sess->resize_pending_h = win->h;
        return;
    }

    if (win->maximized == 0 && local_y >= 0 && local_y < USH_UWM_TITLE_H) {
        sess->dragging = 1;
        sess->drag_window = window_index;
        sess->mouse_packet_seen = 0ULL;
        sess->drag_offset_x = local_x;
        sess->drag_offset_y = local_y;
    }
}

static void ush_uwm_handle_mouse_move(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event) {
    int global_x;
    int global_y;

    if (sess == (ush_uwm_session *)0 || event == (const cleonos_wm_event *)0 ||
        ush_uwm_window_index_valid(window_index) == 0) {
        return;
    }

    global_x = ush_uwm_u64_as_i32(event->arg0);
    global_y = ush_uwm_u64_as_i32(event->arg1);

    if (sess->resizing != 0 && sess->resize_window == window_index && ush_uwm_app_index_valid(window_index) != 0 &&
        sess->windows[window_index].maximized == 0) {
        sess->resize_pending_w = sess->resize_start_w + (global_x - sess->resize_start_x);
        sess->resize_pending_h = sess->resize_start_h + (global_y - sess->resize_start_y);
        return;
    }

    if (sess->dragging != 0 && sess->drag_window == window_index && ush_uwm_app_index_valid(window_index) != 0 &&
        sess->windows[window_index].maximized == 0) {
        (void)ush_uwm_window_move_clamped(sess, window_index, global_x - sess->drag_offset_x,
                                          global_y - sess->drag_offset_y);
    }
}

static int ush_uwm_drive_direct_pointer(ush_uwm_session *sess) {
    cleonos_mouse_state mouse;
    int global_x;
    int global_y;

    if (sess == (ush_uwm_session *)0 || (sess->dragging == 0 && sess->resizing == 0)) {
        return 0;
    }

    ush_zero(&mouse, (u64)sizeof(mouse));
    if (cleonos_sys_mouse_state(&mouse) == 0ULL || mouse.ready == 0ULL) {
        return 0;
    }

    if (mouse.packet_count == sess->mouse_packet_seen) {
        return 0;
    }
    sess->mouse_packet_seen = mouse.packet_count;

    if ((mouse.buttons & 0x1ULL) == 0ULL) {
        if (sess->dragging != 0) {
            sess->dragging = 0;
            sess->drag_window = -1;
        }
        if (sess->resizing != 0) {
            ush_uwm_finish_resize(sess);
        }
        return 1;
    }

    global_x = ush_uwm_u64_as_i32(mouse.x);
    global_y = ush_uwm_u64_as_i32(mouse.y);

    if (sess->resizing != 0 && ush_uwm_app_index_valid(sess->resize_window) != 0 &&
        sess->windows[sess->resize_window].maximized == 0) {
        sess->resize_pending_w = sess->resize_start_w + (global_x - sess->resize_start_x);
        sess->resize_pending_h = sess->resize_start_h + (global_y - sess->resize_start_y);
        return 1;
    }

    if (sess->dragging != 0 && ush_uwm_app_index_valid(sess->drag_window) != 0 &&
        sess->windows[sess->drag_window].maximized == 0) {
        (void)ush_uwm_window_move_clamped(sess, sess->drag_window, global_x - sess->drag_offset_x,
                                          global_y - sess->drag_offset_y);
        return 1;
    }

    return 0;
}

void ush_uwm_handle_event(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event, int *running) {
    if (sess == (ush_uwm_session *)0 || event == (const cleonos_wm_event *)0 || running == (int *)0 ||
        ush_uwm_window_index_valid(window_index) == 0) {
        return;
    }

    switch (event->type) {
    case CLEONOS_WM_EVENT_FOCUS_GAINED:
        if (ush_uwm_app_index_valid(window_index) != 0) {
            int old_active = sess->active_window;
            sess->active_window = window_index;
            if (ush_uwm_app_index_valid(old_active) != 0 && old_active != window_index) {
                ush_uwm_refresh_window(sess, old_active);
            }
            ush_uwm_refresh_window(sess, window_index);
            ush_uwm_refresh_taskbar(sess);
        }
        break;
    case CLEONOS_WM_EVENT_FOCUS_LOST:
        if (sess->drag_window == window_index) {
            sess->dragging = 0;
            sess->drag_window = -1;
        }
        if (sess->resize_window == window_index) {
            ush_uwm_finish_resize(sess);
        }
        break;
    case CLEONOS_WM_EVENT_KEY:
        ush_uwm_handle_key_event(sess, event->arg0, running);
        break;
    case CLEONOS_WM_EVENT_MOUSE_MOVE:
        ush_uwm_handle_mouse_move(sess, window_index, event);
        break;
    case CLEONOS_WM_EVENT_MOUSE_BUTTON:
        ush_uwm_handle_mouse_button(sess, window_index, event);
        break;
    default:
        break;
    }
}

static int ush_uwm_poll_window_events(ush_uwm_session *sess, int window_index, int *running) {
    ush_uwm_window *win;
    u64 budget = 0ULL;
    int handled = 0;

    if (sess == (ush_uwm_session *)0 || running == (int *)0 || ush_uwm_window_index_valid(window_index) == 0) {
        return 0;
    }

    win = &sess->windows[window_index];
    if (win->alive == 0 || win->id == 0ULL) {
        return 0;
    }

    while (budget < USH_UWM_EVENT_BUDGET) {
        cleonos_wm_event event;
        ush_zero(&event, (u64)sizeof(event));
        if (cleonos_sys_wm_poll_event(win->id, &event) == 0ULL) {
            break;
        }

        ush_uwm_handle_event(sess, window_index, &event, running);
        handled++;
        if (*running == 0) {
            break;
        }
        budget++;
    }

    return handled;
}

int ush_uwm_loop(ush_uwm_session *sess) {
    int running = 1;
    int idle_spins = 0;

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    while (running != 0) {
        int i;
        int handled_events = 0;
        int preferred_window = -1;
        u64 now_tick = cleonos_sys_timer_ticks();

        if (now_tick - sess->app_registry_last_tick >= 20ULL) {
            sess->app_registry_last_tick = now_tick;
            if (ush_uwm_refresh_app_registry(sess) != 0) {
                ush_uwm_refresh_taskbar(sess);
                if (sess->start_open != 0) {
                    ush_uwm_refresh_window(sess, USH_UWM_START_INDEX);
                }
            }
        }

        if (sess->dragging != 0 && ush_uwm_window_index_valid(sess->drag_window) != 0) {
            preferred_window = sess->drag_window;
        } else if (sess->resizing != 0 && ush_uwm_window_index_valid(sess->resize_window) != 0) {
            preferred_window = sess->resize_window;
        } else if (sess->start_open != 0 && sess->windows[USH_UWM_START_INDEX].alive != 0) {
            preferred_window = USH_UWM_START_INDEX;
        } else if (ush_uwm_window_index_valid(sess->active_window) != 0) {
            preferred_window = sess->active_window;
        }

        handled_events += ush_uwm_drive_direct_pointer(sess);
        if (preferred_window >= 0) {
            handled_events += ush_uwm_poll_window_events(sess, preferred_window, &running);
        }

        if (running == 0) {
            break;
        }

        for (i = 0; i < (int)USH_UWM_WINDOW_COUNT; i++) {
            if (i == preferred_window) {
                continue;
            }
            handled_events += ush_uwm_poll_window_events(sess, i, &running);
            if (running == 0) {
                break;
            }
        }

        if (running == 0) {
            break;
        }

        if (handled_events != 0 || sess->dragging != 0 || sess->resizing != 0) {
            idle_spins = 0;
            (void)cleonos_sys_yield();
            continue;
        }

        if (idle_spins < USH_UWM_IDLE_SPINS) {
            idle_spins++;
            (void)cleonos_sys_yield();
            continue;
        }

        idle_spins = 0;
        (void)cleonos_sys_sleep_ticks(1ULL);
    }

    return 1;
}
