#include "hbos.h"

static const hbos_file hbos_files[] = {
    {"IPL10", "NAS", "source", 291, "Haribote IPL source placeholder. The host boots through CLeonOS instead."},
    {"HARIBOTE", "SYS", "kernel", 0, "Loaded from /system/hbos/HARIBOTE.SYS."},
    {"A", "HRB", "app", 84, "A sample HRB application."},
    {"HELLO3", "HRB", "app", 101, "hello world application."},
    {"HELLO4", "HRB", "app", 112, "hello world string application."},
    {"HELLO5", "HRB", "app", 78, "hello world assembly HRB application."},
    {"WINHELO", "HRB", "app", 2048, "window hello application."},
    {"WINHELO2", "HRB", "app", 2048, "window hello application."},
    {"WINHELO3", "HRB", "app", 2048, "window hello application."},
    {"STARS", "HRB", "app", 322, "draws stars through the Haribote API."},
    {"LINES", "HRB", "app", 6144, "draws line graphics through api_linewin."},
    {"NOODLE", "HRB", "app", 2304, "classic noodle timer demo."},
    {"TYPE", "HRB", "app", 2048, "prints a file to the console."},
    {"CHKLANG", "HRB", "app", 1024, "prints Haribote language mode."},
    {"IROHA", "HRB", "app", 1024, "prints sample Japanese text."},
    {"README", "TXT", "text", 312, "HariboteOS compatibility layer for CLeonOS. Type help for commands."},
};

const hbos_file *hbos_file_at(hbos_u32 index) {
    if (index >= hbos_file_count()) {
        return (const hbos_file *)0;
    }
    return &hbos_files[index];
}

hbos_u32 hbos_file_count(void) {
    return (hbos_u32)(sizeof(hbos_files) / sizeof(hbos_files[0]));
}

hbos_u32 hbos_file_size_for_state(const hbos_state *state, const hbos_file *file) {
    if (file == (const hbos_file *)0) {
        return 0U;
    }

    if (hbos_streq_ci(file->name, "HARIBOTE") != 0 && hbos_streq_ci(file->ext, "SYS") != 0 &&
        state != (const hbos_state *)0 && state->haribote_kernel_loaded != 0) {
        return state->haribote_kernel_size;
    }

    return file->size;
}

const char *hbos_file_content_for_state(const hbos_state *state, const hbos_file *file) {
    (void)state;
    if (file == (const hbos_file *)0) {
        return "";
    }

    if (hbos_streq_ci(file->name, "HARIBOTE") != 0 && hbos_streq_ci(file->ext, "SYS") != 0) {
        return "HARIBOTE.SYS is the loaded 32-bit HariboteOS kernel image. Binary output is not printed.";
    }

    return file->content;
}

static int hbos_match_83(const hbos_file *file, const char *name) {
    char wanted[12];
    char have[12];
    hbos_u32 i;
    hbos_u32 j = 0U;

    if (file == (const hbos_file *)0 || name == (const char *)0) {
        return 0;
    }

    for (i = 0U; i < 11U; i++) {
        wanted[i] = ' ';
        have[i] = ' ';
    }
    wanted[11] = '\0';
    have[11] = '\0';

    for (i = 0U; file->name[i] != '\0' && i < 8U; i++) {
        have[i] = file->name[i];
    }
    for (i = 0U; file->ext != (const char *)0 && file->ext[i] != '\0' && i < 3U; i++) {
        have[8U + i] = file->ext[i];
    }

    for (i = 0U; name[i] != '\0'; i++) {
        char ch = name[i];
        if (j >= 11U) {
            return 0;
        }
        if (ch == '.' && j <= 8U) {
            j = 8U;
            continue;
        }
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        wanted[j++] = ch;
    }

    return (hbos_streq_ci(wanted, have) != 0) ? 1 : 0;
}

const hbos_file *hbos_find_file(const char *name) {
    hbos_u32 i;
    for (i = 0U; i < hbos_file_count(); i++) {
        if (hbos_match_83(&hbos_files[i], name) != 0) {
            return &hbos_files[i];
        }
    }
    return (const hbos_file *)0;
}

void hbos_format_83_name(const hbos_file *file, char *out, hbos_u32 out_size) {
    hbos_u32 p = 0U;
    hbos_u32 i;

    if (out == (char *)0 || out_size == 0U) {
        return;
    }
    out[0] = '\0';
    if (file == (const hbos_file *)0) {
        return;
    }

    for (i = 0U; i < 8U && p + 1U < out_size; i++) {
        char ch = (file->name[i] != '\0') ? file->name[i] : ' ';
        out[p++] = ch;
    }
    if (p + 1U < out_size) {
        out[p++] = '.';
    }
    for (i = 0U; i < 3U && p + 1U < out_size; i++) {
        char ch = (file->ext != (const char *)0 && file->ext[i] != '\0') ? file->ext[i] : ' ';
        out[p++] = ch;
    }
    out[p] = '\0';
}
