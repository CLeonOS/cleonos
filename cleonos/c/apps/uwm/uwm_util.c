#include "uwm.h"

int ush_uwm_window_index_valid(int index) {
    return (index >= 0 && index < (int)USH_UWM_WINDOW_COUNT) ? 1 : 0;
}

int ush_uwm_app_index_valid(int index) {
    return (index >= 0 && index < (int)USH_UWM_APP_COUNT) ? 1 : 0;
}

int ush_uwm_clampi(int value, int min_value, int max_value) {
    if (max_value < min_value) {
        return min_value;
    }

    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

int ush_uwm_u64_as_i32(u64 raw) {
    i64 value = (i64)raw;

    if (value < (-2147483647LL - 1LL)) {
        return -2147483647 - 1;
    }

    if (value > 2147483647LL) {
        return 2147483647;
    }

    return (int)value;
}

void ush_uwm_drain_startup_keys(void) {
    u64 drained = 0ULL;

    while (drained < USH_UWM_STARTUP_KEY_DRAIN_MAX) {
        if (cleonos_sys_kbd_get_char() == (u64)-1) {
            break;
        }
        drained++;
    }
}

int ush_uwm_app_registry_running(ush_uwm_session *sess, int index) {
    cleonos_proc_snapshot snap;
    u64 pid;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return 0;
    }

    pid = sess->app_pids[index];
    if (pid == 0ULL || pid == (u64)-1) {
        sess->app_pids[index] = 0ULL;
        sess->app_states[index] = CLEONOS_PROC_STATE_UNUSED;
        return 0;
    }

    ush_zero(&snap, (u64)sizeof(snap));
    if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
        sess->app_pids[index] = 0ULL;
        sess->app_states[index] = CLEONOS_PROC_STATE_UNUSED;
        return 0;
    }

    sess->app_states[index] = snap.state;
    if (snap.state == CLEONOS_PROC_STATE_EXITED || snap.state == CLEONOS_PROC_STATE_UNUSED) {
        sess->app_pids[index] = 0ULL;
        return 0;
    }

    return 1;
}

int ush_uwm_refresh_app_registry(ush_uwm_session *sess) {
    int i;
    int changed = 0;

    if (sess == (ush_uwm_session *)0) {
        return 0;
    }

    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        u64 old_pid = sess->app_pids[i];
        u64 old_state = sess->app_states[i];
        (void)ush_uwm_app_registry_running(sess, i);
        if (old_pid != sess->app_pids[i] || old_state != sess->app_states[i]) {
            changed = 1;
        }
    }

    return changed;
}
