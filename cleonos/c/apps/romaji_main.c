#include "cmd_runtime.h"

#include <stdio.h>
#include <string.h>

#define ROMAJI_DICT_PATH "/system/inputm/romaji.db"

typedef struct romaji_pair {
    const char *key;
    const char *kana;
} romaji_pair;

static const romaji_pair romaji_table[] = {
    {"a", "あ"},     {"i", "い"},     {"u", "う"},     {"e", "え"},     {"o", "お"},
    {"ka", "か"},    {"ki", "き"},    {"ku", "く"},    {"ke", "け"},    {"ko", "こ"},
    {"sa", "さ"},    {"shi", "し"},   {"si", "し"},    {"su", "す"},    {"se", "せ"},
    {"so", "そ"},    {"ta", "た"},    {"chi", "ち"},   {"ti", "ち"},    {"tsu", "つ"},
    {"tu", "つ"},    {"te", "て"},    {"to", "と"},    {"na", "な"},    {"ni", "に"},
    {"nu", "ぬ"},    {"ne", "ね"},    {"no", "の"},    {"ha", "は"},    {"hi", "ひ"},
    {"fu", "ふ"},    {"hu", "ふ"},    {"he", "へ"},    {"ho", "ほ"},    {"ma", "ま"},
    {"mi", "み"},    {"mu", "む"},    {"me", "め"},    {"mo", "も"},    {"ya", "や"},
    {"yu", "ゆ"},    {"yo", "よ"},    {"ra", "ら"},    {"ri", "り"},    {"ru", "る"},
    {"re", "れ"},    {"ro", "ろ"},    {"wa", "わ"},    {"wo", "を"},    {"n", "ん"},
    {"ga", "が"},    {"gi", "ぎ"},    {"gu", "ぐ"},    {"ge", "げ"},    {"go", "ご"},
    {"za", "ざ"},    {"ji", "じ"},    {"zi", "じ"},    {"zu", "ず"},    {"ze", "ぜ"},
    {"zo", "ぞ"},    {"da", "だ"},    {"di", "ぢ"},    {"du", "づ"},    {"de", "で"},
    {"do", "ど"},    {"ba", "ば"},    {"bi", "び"},    {"bu", "ぶ"},    {"be", "べ"},
    {"bo", "ぼ"},    {"pa", "ぱ"},    {"pi", "ぴ"},    {"pu", "ぷ"},    {"pe", "ぺ"},
    {"po", "ぽ"},
};

static void romaji_usage(void) {
    puts("usage: romaji [list|use <index>|status <text>|test <key>|table]");
    puts("hotkey: Ctrl+Shift+Space switches input method");
    puts("input: type romaji, press Space or 1-5 to commit kana, +/- changes candidate page");
    puts("scope: kana only, no kanji conversion");
}

static void romaji_list(void) {
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

static int romaji_test_key(const char *key) {
    unsigned int i;

    if (key == 0 || key[0] == '\0') {
        puts("romaji: missing key");
        return 1;
    }

    for (i = 0U; i < (unsigned int)(sizeof(romaji_table) / sizeof(romaji_table[0])); i++) {
        if (strcmp(key, romaji_table[i].key) == 0) {
            printf("%s -> %s\n", key, romaji_table[i].kana);
            return 0;
        }
    }

    printf("%s: no kana mapping\n", key);
    return 1;
}

static void romaji_print_table(void) {
    unsigned int i;

    for (i = 0U; i < (unsigned int)(sizeof(romaji_table) / sizeof(romaji_table[0])); i++) {
        printf("%-4s %s", romaji_table[i].key, romaji_table[i].kana);
        if ((i % 5U) == 4U) {
            putchar('\n');
        } else {
            printf("   ");
        }
    }
    if ((i % 5U) != 0U) {
        putchar('\n');
    }
}

int cleonos_app_main(void) {
    char arg[USH_ARG_MAX];
    char first[48];
    const char *rest = "";
    u64 argc;
    u64 ai;
    u64 idx;

    (void)cleonos_sys_inputm_register_rule("RomajiJP", "/shell/inputm/romaji.elf", ROMAJI_DICT_PATH, "ROMAJI:",
                                           CLEONOS_INPUTM_FLAG_JAPANESE_ROMAJI |
                                               CLEONOS_INPUTM_FLAG_RULE_LOWERCASE |
                                               CLEONOS_INPUTM_FLAG_RULE_SPLIT |
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
        romaji_list();
        return 0;
    }

    if (strcmp(arg, "help") == 0 || strcmp(arg, "--help") == 0) {
        romaji_usage();
        return 0;
    }

    if (strcmp(arg, "table") == 0) {
        romaji_print_table();
        return 0;
    }

    first[0] = '\0';
    (void)ush_split_first_and_rest(arg, first, sizeof(first), &rest);
    while (*rest == ' ') {
        rest++;
    }

    if (strcmp(first, "use") == 0) {
        if (ush_parse_u64_dec(rest, &idx) == 0) {
            puts("romaji: invalid index");
            return 1;
        }
        if (cleonos_sys_inputm_select(idx) == 0ULL) {
            puts("romaji: select failed");
            return 1;
        }
        romaji_list();
        return 0;
    }

    if (strcmp(first, "status") == 0) {
        return (cleonos_sys_tty_status_set(rest) != 0ULL) ? 0 : 1;
    }

    if (strcmp(first, "test") == 0) {
        return romaji_test_key(rest);
    }

    romaji_usage();
    return 1;
}
