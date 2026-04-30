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

int ush_uwm_local_window_index_by_id(const ush_uwm_session *sess, u64 window_id) {
    int i;

    if (sess == (const ush_uwm_session *)0 || window_id == 0ULL) {
        return -1;
    }

    for (i = 0; i < (int)USH_UWM_WINDOW_COUNT; i++) {
        if (sess->windows[i].id == window_id) {
            return i;
        }
    }

    return -1;
}

int ush_uwm_taskbar_window_hidden(const ush_uwm_session *sess, u64 window_id) {
    if (sess == (const ush_uwm_session *)0 || window_id == 0ULL) {
        return 1;
    }

    if (window_id == sess->windows[USH_UWM_TASKBAR_INDEX].id || window_id == sess->windows[USH_UWM_START_INDEX].id) {
        return 1;
    }

    return 0;
}

static const char *ush_uwm_basename(const char *path) {
    const char *base = path;
    u64 i;

    if (path == (const char *)0 || path[0] == '\0') {
        return "WINDOW";
    }

    for (i = 0ULL; path[i] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            base = path + i + 1ULL;
        }
    }

    return (base[0] != '\0') ? base : path;
}

static void ush_uwm_append_u64_dec(char *dst, u64 dst_size, u64 value) {
    char tmp[24];
    u64 len = 0ULL;
    u64 pos;

    if (dst == (char *)0 || dst_size == 0ULL) {
        return;
    }

    pos = ush_strlen(dst);
    if (pos >= dst_size) {
        return;
    }

    if (value == 0ULL) {
        if (pos + 1ULL < dst_size) {
            dst[pos++] = '0';
            dst[pos] = '\0';
        }
        return;
    }

    while (value != 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }
    while (len > 0ULL && pos + 1ULL < dst_size) {
        dst[pos++] = tmp[--len];
    }
    dst[pos] = '\0';
}

void ush_uwm_taskbar_window_label(const ush_uwm_session *sess, const cleonos_wm_snapshot *snap, char *out_label,
                                  u64 out_size) {
    int local_index;
    cleonos_proc_snapshot proc;

    if (out_label == (char *)0 || out_size == 0ULL) {
        return;
    }
    out_label[0] = '\0';

    if (snap == (const cleonos_wm_snapshot *)0 || snap->window_id == 0ULL) {
        ush_copy(out_label, out_size, "WINDOW");
        return;
    }

    local_index = ush_uwm_local_window_index_by_id(sess, snap->window_id);
    if (ush_uwm_app_index_valid(local_index) != 0 && sess->windows[local_index].title[0] != '\0') {
        ush_copy(out_label, out_size, sess->windows[local_index].title);
        return;
    }

    ush_zero(&proc, (u64)sizeof(proc));
    if (snap->owner_pid != 0ULL && cleonos_sys_proc_snapshot(snap->owner_pid, &proc, (u64)sizeof(proc)) != 0ULL &&
        proc.path[0] != '\0') {
        ush_copy(out_label, out_size, ush_uwm_basename(proc.path));
        return;
    }

    ush_copy(out_label, out_size, "WIN ");
    ush_uwm_append_u64_dec(out_label, out_size, snap->window_id);
}
