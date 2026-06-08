#include "hbos.h"
#include <stdlib.h>
#include <string.h>

void hbos_ascii_upper(char *text) {
    hbos_u32 i = 0U;
    if (text == (char *)0) {
        return;
    }
    while (text[i] != '\0') {
        if (text[i] >= 'a' && text[i] <= 'z') {
            text[i] = (char)(text[i] - 'a' + 'A');
        }
        i++;
    }
}

static char hbos_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - 'a' + 'A');
    }
    return ch;
}

int hbos_streq_ci(const char *a, const char *b) {
    hbos_u32 i = 0U;
    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (hbos_upper_char(a[i]) != hbos_upper_char(b[i])) {
            return 0;
        }
        i++;
    }
    return (a[i] == '\0' && b[i] == '\0') ? 1 : 0;
}

int hbos_starts_with_ci(const char *text, const char *prefix) {
    hbos_u32 i = 0U;
    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }
    while (prefix[i] != '\0') {
        if (hbos_upper_char(text[i]) != hbos_upper_char(prefix[i])) {
            return 0;
        }
        i++;
    }
    return 1;
}

const char *hbos_skip_spaces(const char *text) {
    if (text == (const char *)0) {
        return "";
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

void hbos_split_first(const char *line, char *cmd, hbos_u32 cmd_size, const char **rest) {
    hbos_u32 i = 0U;
    const char *p = hbos_skip_spaces(line);

    if (cmd != (char *)0 && cmd_size > 0U) {
        cmd[0] = '\0';
    }
    if (rest != (const char **)0) {
        *rest = "";
    }
    if (cmd == (char *)0 || cmd_size == 0U) {
        return;
    }

    while (p[i] != '\0' && p[i] != ' ' && p[i] != '\t' && i + 1U < cmd_size) {
        cmd[i] = p[i];
        i++;
    }
    cmd[i] = '\0';
    while (p[i] != '\0' && p[i] != ' ' && p[i] != '\t') {
        i++;
    }
    if (rest != (const char **)0) {
        *rest = hbos_skip_spaces(p + i);
    }
}

static void hbos_copy(char *dst, hbos_u32 dst_size, const char *src) {
    hbos_u32 i = 0U;
    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

void hbos_terminal_write(const char *text) {
    hbos_u32 len = 0U;

    if (text == (const char *)0) {
        return;
    }

    while (text[len] != '\0') {
        len++;
    }

    if (len > 0U) {
        (void)cleonos_sys_tty_write(text, (hbos_u64)len);
    }
}

void hbos_put_history(hbos_state *state, const char *text, hbos_u32 color) {
    hbos_u32 i;
    (void)color;

    if (state == (hbos_state *)0 || text == (const char *)0) {
        return;
    }
    if (state->history_count < HBOS_HISTORY_MAX) {
        i = state->history_count++;
    } else {
        for (i = 1U; i < HBOS_HISTORY_MAX; i++) {
            hbos_copy(state->history[i - 1U], HBOS_CONSOLE_COLS + 1U, state->history[i]);
            state->history_color[i - 1U] = state->history_color[i];
        }
        i = HBOS_HISTORY_MAX - 1U;
    }
    hbos_copy(state->history[i], HBOS_CONSOLE_COLS + 1U, text);
    state->history_color[i] = color;
    state->dirty = 1;

    if (state->terminal_only != 0) {
        hbos_terminal_write(text);
        hbos_terminal_write("\n");
        state->terminal_flushed = state->history_count;
        state->prompt_pending = 1;
    }
}

void hbos_put_history_fmt_u32(hbos_state *state, const char *prefix, hbos_u32 value, const char *suffix,
                              hbos_u32 color) {
    char out[HBOS_CONSOLE_COLS + 1U];
    char rev[12];
    hbos_u32 p = 0U;
    hbos_u32 n = 0U;
    hbos_u32 i;

    if (prefix != (const char *)0) {
        while (prefix[p] != '\0' && p + 1U < sizeof(out)) {
            out[p] = prefix[p];
            p++;
        }
    }
    if (value == 0U) {
        rev[n++] = '0';
    } else {
        while (value > 0U && n < sizeof(rev)) {
            rev[n++] = (char)('0' + (value % 10U));
            value /= 10U;
        }
    }
    for (i = 0U; i < n && p + 1U < sizeof(out); i++) {
        out[p++] = rev[n - 1U - i];
    }
    if (suffix != (const char *)0) {
        i = 0U;
        while (suffix[i] != '\0' && p + 1U < sizeof(out)) {
            out[p++] = suffix[i++];
        }
    }
    out[p] = '\0';
    hbos_put_history(state, out, color);
}

void hbos_sleep(hbos_u64 ms) {
    if (ms == 0ULL) {
        return;
    }

    (void)cleonos_sys_sleep_ms(ms);
}

int hbos_poll_char(void) {
    hbos_u64 key = cleonos_sys_kbd_get_char();
    if (key == (hbos_u64)-1) {
        return -1;
    }
    return (int)(key & 0xFFU);
}

int hbos_init(hbos_state *state) {
    hbos_u32 i;
    if (state == (hbos_state *)0) {
        return 0;
    }
    memset(state, 0, sizeof(*state));

    for (i = 0U; i < HBOS_HISTORY_MAX; i++) {
        state->history[i][0] = '\0';
        state->history_color[i] = HBOS_COLOR_CONSOLE_TEXT;
    }
    state->history_count = 0U;
    state->input[0] = '\0';
    state->input_len = 0U;
    state->running = 1;
    state->dirty = 1;
    state->show_about = 0;
    state->frame_no = 0ULL;
    state->terminal_flushed = 0U;
    state->prompt_pending = 0;
    state->terminal_only = 1;
    if (hbos_read_file_alloc(HBOS_KERNEL_PATH, &state->haribote_kernel, &state->haribote_kernel_size,
                             HBOS_KERNEL_MAX_BYTES) == 0) {
        hbos_put_history(state, "hbos: missing Haribote kernel image: /system/hbos/HARIBOTE.SYS", HBOS_COLOR_WARN);
        hbos_put_history(state, "hbos: rebuild ramdisk with Haribote assets, then run hbos again.",
                         HBOS_COLOR_CONSOLE_DIM);
        return 0;
    }
    state->haribote_kernel_loaded = 1;
    hbos_put_history(state, "HariboteOS 0.27f kernel image loaded from /system/hbos/HARIBOTE.SYS", HBOS_COLOR_OK);
    hbos_put_history_fmt_u32(state, "HARIBOTE.SYS bytes: ", state->haribote_kernel_size, "", HBOS_COLOR_CONSOLE_DIM);
    hbos_put_history(state, "HariboteOS 0.27f compatible user-mode terminal host", HBOS_COLOR_OK);
    hbos_put_history(state, "Commands: mem cls dir exit start ncst langmode; apps: a hello3 hello4 hello5 stars lines noodle.",
                     HBOS_COLOR_CONSOLE_DIM);
    hbos_put_history(state, "CLeonOS-only command: exit2cleonos.", HBOS_COLOR_CONSOLE_DIM);
    return 1;
}

void hbos_shutdown(hbos_state *state) {
    if (state == (hbos_state *)0) {
        return;
    }
    if (state->haribote_kernel != (hbos_u8 *)0) {
        hbos_free_file_alloc(state->haribote_kernel, state->haribote_kernel_size);
        state->haribote_kernel = (hbos_u8 *)0;
    }
    state->haribote_kernel_size = 0U;
    state->haribote_kernel_loaded = 0;
    if (state->present_pixels != (hbos_u32 *)0) {
        free(state->present_pixels);
        state->present_pixels = (hbos_u32 *)0;
    }
    if (state->pixels != (hbos_u32 *)0) {
        free(state->pixels);
        state->pixels = (hbos_u32 *)0;
    }
    state->present_w = 0U;
    state->present_h = 0U;
}
