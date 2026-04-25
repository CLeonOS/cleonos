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
