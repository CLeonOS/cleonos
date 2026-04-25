#include "uwm.h"

static void ush_uwm_handle_key_event(ush_uwm_session *sess, u64 key, int *running) {
    int idx = 0;
    int dx = 0;
    int dy = 0;

    if (sess == (ush_uwm_session *)0 || running == (int *)0) {
        return;
    }

    if (key == (u64)'q' || key == (u64)'Q') {
        *running = 0;
        return;
    }

    if (key == (u64)'\t') {
        ush_uwm_focus_next(sess);
        return;
    }

    if (key >= (u64)'1' && key <= (u64)'3') {
        int candidate = (int)(key - (u64)'1');
        if (ush_uwm_window_index_valid(candidate) != 0) {
            ush_uwm_set_active(sess, candidate);
        }
        return;
    }

    idx = sess->active_window;
    if (ush_uwm_window_index_valid(idx) == 0 || sess->windows[idx].alive == 0) {
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

static void ush_uwm_handle_mouse_button(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event) {
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

    buttons = event->arg0;
    changed = event->arg1;
    local_x = ush_uwm_u64_as_i32(event->arg2);
    local_y = ush_uwm_u64_as_i32(event->arg3);
    left_changed = ((changed & 0x1ULL) != 0ULL) ? 1 : 0;
    left_down = ((buttons & 0x1ULL) != 0ULL) ? 1 : 0;

    if (left_changed == 0) {
        return;
    }

    if (left_down != 0) {
        if (local_y >= 0 && local_y < USH_UWM_TITLE_DRAG_HEIGHT) {
            sess->dragging = 1;
            sess->drag_window = window_index;
            sess->drag_offset_x = local_x;
            sess->drag_offset_y = local_y;
            sess->active_window = window_index;
        }
    } else if (sess->dragging != 0 && sess->drag_window == window_index) {
        sess->dragging = 0;
    }
}

static void ush_uwm_handle_mouse_move(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event) {
    int global_x;
    int global_y;
    int new_x;
    int new_y;

    if (sess == (ush_uwm_session *)0 || event == (const cleonos_wm_event *)0 ||
        ush_uwm_window_index_valid(window_index) == 0) {
        return;
    }

    if (sess->dragging == 0 || sess->drag_window != window_index) {
        return;
    }

    global_x = ush_uwm_u64_as_i32(event->arg0);
    global_y = ush_uwm_u64_as_i32(event->arg1);
    new_x = global_x - sess->drag_offset_x;
    new_y = global_y - sess->drag_offset_y;
    (void)ush_uwm_window_move_clamped(sess, window_index, new_x, new_y);
}

void ush_uwm_handle_event(ush_uwm_session *sess, int window_index, const cleonos_wm_event *event, int *running) {
    if (sess == (ush_uwm_session *)0 || event == (const cleonos_wm_event *)0 || running == (int *)0 ||
        ush_uwm_window_index_valid(window_index) == 0) {
        return;
    }

    switch (event->type) {
    case CLEONOS_WM_EVENT_FOCUS_GAINED:
        sess->active_window = window_index;
        break;
    case CLEONOS_WM_EVENT_FOCUS_LOST:
        if (sess->drag_window == window_index) {
            sess->dragging = 0;
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

int ush_uwm_loop(ush_uwm_session *sess) {
    int running = 1;

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    while (running != 0) {
        int i;

        for (i = 0; i < (int)USH_UWM_WINDOW_COUNT; i++) {
            ush_uwm_window *win = &sess->windows[i];
            u64 budget = 0ULL;

            if (win->alive == 0 || win->id == 0ULL) {
                continue;
            }

            while (budget < USH_UWM_EVENT_BUDGET) {
                cleonos_wm_event event;
                ush_zero(&event, (u64)sizeof(event));
                if (cleonos_sys_wm_poll_event(win->id, &event) == 0ULL) {
                    break;
                }

                ush_uwm_handle_event(sess, i, &event, &running);
                if (running == 0) {
                    break;
                }

                budget++;
            }

            if (running == 0) {
                break;
            }
        }

        if (running == 0) {
            break;
        }

        (void)cleonos_sys_yield();
    }

    return 1;
}
