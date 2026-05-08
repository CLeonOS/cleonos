#include "cmd_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYMBOLS_DICT_PATH "/system/inputm/symbols.db"

static void symbols_usage(void) {
    puts("usage: symbols [list|use <index>|status <text>|test <key>]");
    puts("hotkey: Ctrl+Shift+Space switches input method");
    puts("examples: alpha -> α, sqrt -> √, arrow -> →");
}

static void symbols_list(void) {
    u64 count = cleonos_sys_inputm_count();
    u64 current = cleonos_sys_inputm_current();
    u64 i;

    printf("input methods: %llu\n", count);
    for (i = 0ULL; i < count; i++) {
        cleonos_inputm_info info;
        if (cleonos_sys_inputm_info(i, &info) != 0ULL) {
            printf("%c %llu: %s path=%s rule=%s label=%s flags=0x%llx\n", (i == current) ? '*' : ' ', i, info.name,
                   info.path, info.rule_path, info.label, info.flags);
        }
    }
}

static int symbols_test_key(const char *key) {
    u64 size = cleonos_sys_fs_stat_size(SYMBOLS_DICT_PATH);
    char *data;
    u64 got;
    u64 pos = 0ULL;

    if (key == 0 || key[0] == '\0' || size == (u64)-1 || size == 0ULL) {
        puts("symbols: dictionary not available");
        return 1;
    }

    data = (char *)malloc((size_t)size + 1U);
    if (data == 0) {
        puts("symbols: out of memory");
        return 1;
    }

    got = cleonos_sys_fs_read(SYMBOLS_DICT_PATH, data, size);
    data[got] = '\0';

    while (pos < got) {
        u64 line = pos;
        u64 k = 0ULL;

        while (pos < got && data[pos] != '\n' && data[pos] != '\r') {
            pos++;
        }
        while (pos < got && (data[pos] == '\n' || data[pos] == '\r')) {
            data[pos++] = '\0';
        }

        if (data[line] == '#') {
            continue;
        }

        while (data[line + k] != '\0' && key[k] != '\0' && data[line + k] == key[k]) {
            k++;
        }
        if (key[k] == '\0' && data[line + k] == '\t') {
            printf("%s -> %s\n", key, data + line + k + 1ULL);
            free(data);
            return 0;
        }
    }

    printf("%s: no symbol mapping\n", key);
    free(data);
    return 1;
}

int cleonos_app_main(void) {
    char arg[USH_ARG_MAX];
    char first[48];
    const char *rest = "";
    u64 argc;
    u64 ai;
    u64 idx;

    (void)cleonos_sys_inputm_register_rule("Symbols", "/shell/inputm/symbols.elf", SYMBOLS_DICT_PATH, "SYM:",
                                           CLEONOS_INPUTM_FLAG_RULE_LOWERCASE |
                                               CLEONOS_INPUTM_FLAG_RULE_COMMIT_RAW);

    arg[0] = '\0';
    argc = cleonos_sys_proc_argc();
    for (ai = 1ULL; ai < argc; ai++) {
        char part[64];
        (void)cleonos_sys_proc_argv(ai, part, sizeof(part));
        if (arg[0] != '\0') {
            strncat(arg, " ", sizeof(arg) - strlen(arg) - 1U);
        }
        strncat(arg, part, sizeof(arg) - strlen(arg) - 1U);
    }

    ush_trim_line(arg);
    if (arg[0] == '\0' || strcmp(arg, "list") == 0) {
        symbols_list();
        return 0;
    }

    if (strcmp(arg, "help") == 0 || strcmp(arg, "--help") == 0) {
        symbols_usage();
        return 0;
    }

    first[0] = '\0';
    (void)ush_split_first_and_rest(arg, first, sizeof(first), &rest);
    while (*rest == ' ') {
        rest++;
    }

    if (strcmp(first, "use") == 0) {
        if (ush_parse_u64_dec(rest, &idx) == 0) {
            puts("symbols: invalid index");
            return 1;
        }
        if (cleonos_sys_inputm_select(idx) == 0ULL) {
            puts("symbols: select failed");
            return 1;
        }
        symbols_list();
        return 0;
    }

    if (strcmp(first, "status") == 0) {
        return (cleonos_sys_tty_status_set(rest) != 0ULL) ? 0 : 1;
    }

    if (strcmp(first, "test") == 0) {
        return symbols_test_key(rest);
    }

    symbols_usage();
    return 1;
}
