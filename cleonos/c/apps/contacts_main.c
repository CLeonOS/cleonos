#include "user_cmd_common.h"
#include <stdio.h>

#define CONTACTS_LINE_MAX 512U

typedef struct contacts_entry {
    char name[64];
    char phone[64];
    char email[96];
} contacts_entry;

static void contacts_default_path(char *out_path, u64 out_size) {
    cleonos_user_info info;

    if (out_path == (char *)0 || out_size == 0ULL) {
        return;
    }

    if (cleonos_sys_user_current(&info) != 0ULL && info.logged_in != 0ULL && info.home[0] != '\0') {
        (void)snprintf(out_path, (size_t)out_size, "%s/contacts.txt", info.home);
        return;
    }

    ush_copy(out_path, out_size, "/temp/contacts.txt");
}

static void contacts_trim_newline(char *text) {
    u64 len;

    if (text == (char *)0) {
        return;
    }

    len = ush_strlen(text);
    while (len > 0ULL && (text[len - 1ULL] == '\n' || text[len - 1ULL] == '\r')) {
        text[len - 1ULL] = '\0';
        len--;
    }
}

static int contacts_parse_line(const char *line, contacts_entry *out) {
    char tmp[CONTACTS_LINE_MAX];
    const char *p;
    char *parts[3];
    int part = 0;

    if (line == (const char *)0 || out == (contacts_entry *)0) {
        return 0;
    }

    ush_zero(out, (u64)sizeof(*out));
    ush_copy(tmp, (u64)sizeof(tmp), line);

    p = tmp;
    while (*p != '\0' && part < 3) {
        char *start = (char *)p;
        while (*p != '\0' && *p != '|') {
            p++;
        }
        if (*p == '|') {
            * (char *) p = '\0';
            p++;
        }
        parts[part++] = start;
    }

    if (part < 2) {
        return 0;
    }

    ush_copy(out->name, (u64)sizeof(out->name), parts[0]);
    ush_copy(out->phone, (u64)sizeof(out->phone), parts[1]);
    if (part >= 3) {
        ush_copy(out->email, (u64)sizeof(out->email), parts[2]);
    }
    return out->name[0] != '\0' ? 1 : 0;
}

static void contacts_format_line(const contacts_entry *entry, char *out, u64 out_size) {
    if (out == (char *)0 || out_size == 0ULL || entry == (const contacts_entry *)0) {
        return;
    }

    (void)snprintf(out, (size_t)out_size, "%s|%s|%s\n", entry->name, entry->phone, entry->email);
}

static int contacts_split_three(const char *arg, char *first, u64 first_size, char *second, u64 second_size,
                                char *third, u64 third_size) {
    const char *rest = "";
    const char *tail = "";

    if (ush_split_first_and_rest(arg, first, first_size, &rest) == 0) {
        return 0;
    }
    if (ush_split_first_and_rest(rest, second, second_size, &tail) == 0) {
        return 0;
    }
    if (third != (char *)0 && third_size != 0ULL) {
        ush_copy(third, third_size, tail);
        ush_trim_line(third);
    }
    return 1;
}

static int contacts_read_all(const char *path, char *out, u64 out_size, u64 *out_len) {
    u64 size;
    u64 got;

    if (path == (const char *)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out[0] = '\0';
    if (out_len != (u64 *)0) {
        *out_len = 0ULL;
    }

    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1 || size + 1ULL > out_size) {
        return 0;
    }

    if (size == 0ULL) {
        return 1;
    }

    got = cleonos_sys_fs_read(path, out, size);
    if (got != size) {
        return 0;
    }

    out[got] = '\0';
    if (out_len != (u64 *)0) {
        *out_len = got;
    }
    return 1;
}

static int contacts_write_all(const char *path, const char *data) {
    return (cleonos_sys_fs_write(path, data, ush_strlen(data)) != 0ULL) ? 1 : 0;
}

static int contacts_append_entry(const char *path, const contacts_entry *entry) {
    char line[CONTACTS_LINE_MAX];

    contacts_format_line(entry, line, (u64)sizeof(line));
    if (line[0] == '\0') {
        return 0;
    }
    return (cleonos_sys_fs_append(path, line, ush_strlen(line)) != 0ULL) ? 1 : 0;
}

static int contacts_load_entries(const char *path, contacts_entry *entries, size_t max_entries, size_t *out_count) {
    static char db[8192];
    u64 len = 0ULL;
    u64 start = 0ULL;
    size_t count = 0U;
    u64 i;

    if (out_count != (size_t *)0) {
        *out_count = 0U;
    }

    if (contacts_read_all(path, db, (u64)sizeof(db), &len) == 0) {
        return 0;
    }

    for (i = 0ULL; i <= len && count < max_entries; i++) {
        if (i == len || db[i] == '\n') {
            char line[CONTACTS_LINE_MAX];
            u64 line_len = i - start;
            contacts_entry entry;

            if (line_len >= (u64)sizeof(line)) {
                line_len = (u64)sizeof(line) - 1ULL;
            }
            (void)memcpy(line, db + start, (size_t)line_len);
            line[line_len] = '\0';
            contacts_trim_newline(line);

            if (contacts_parse_line(line, &entry) != 0) {
                entries[count++] = entry;
            }

            start = i + 1ULL;
        }
    }

    if (out_count != (size_t *)0) {
        *out_count = count;
    }
    return 1;
}

static int contacts_print_entries(const char *path) {
    contacts_entry entries[64];
    size_t count = 0U;
    size_t i;

    if (contacts_load_entries(path, entries, 64U, &count) == 0) {
        ush_writeln_i18n("contacts: read failed", "contacts: 读取失败");
        return 0;
    }

    if (count == 0U) {
        ush_writeln_i18n("contacts: empty", "contacts: 为空");
        return 1;
    }

    for (i = 0U; i < count; i++) {
        ush_write(entries[i].name);
        ush_write("  ");
        ush_write(entries[i].phone);
        if (entries[i].email[0] != '\0') {
            ush_write("  ");
            ush_write(entries[i].email);
        }
        ush_write_char('\n');
    }

    return 1;
}

static int contacts_remove_entry(const char *path, const char *name) {
    contacts_entry entries[64];
    contacts_entry kept[64];
    size_t count = 0U;
    size_t kept_count = 0U;
    char db[8192];
    char line[CONTACTS_LINE_MAX];
    size_t i;

    if (name == (const char *)0 || name[0] == '\0') {
        return 0;
    }

    if (contacts_load_entries(path, entries, 64U, &count) == 0) {
        return 0;
    }

    for (i = 0U; i < count; i++) {
        if (ush_streq(entries[i].name, name) == 0) {
            kept[kept_count++] = entries[i];
        }
    }

    db[0] = '\0';
    for (i = 0U; i < kept_count; i++) {
        contacts_format_line(&kept[i], line, (u64)sizeof(line));
        ush_copy(db + ush_strlen(db), (u64)(sizeof(db) - ush_strlen(db)), line);
    }

    return contacts_write_all(path, db);
}

static int contacts_handle(const char *path, const char *arg) {
    char cmd[32];
    const char *rest = "";
    contacts_entry entry;

    if (arg == (const char *)0 || arg[0] == '\0' || ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
        return contacts_print_entries(path);
    }

    if (ush_streq(cmd, "list") != 0 || ush_streq(cmd, "show") != 0) {
        return contacts_print_entries(path);
    }

    if (ush_streq(cmd, "path") != 0) {
        ush_writeln(path);
        return 1;
    }

    if (ush_streq(cmd, "clear") != 0) {
        return contacts_write_all(path, "");
    }

    if (ush_streq(cmd, "remove") != 0 || ush_streq(cmd, "rm") != 0) {
        return contacts_remove_entry(path, rest);
    }

    if (ush_streq(cmd, "add") != 0 || ush_streq(cmd, "set") != 0) {
        char first[64];
        char second[64];
        char third[96];
        char line[CONTACTS_LINE_MAX];

        ush_zero(&entry, (u64)sizeof(entry));
        if (contacts_split_three(rest, first, (u64)sizeof(first), second, (u64)sizeof(second), third,
                                 (u64)sizeof(third)) == 0) {
            ush_writeln_i18n("contacts: usage contacts add <name> <phone> [email]",
                             "contacts: 用法 contacts add <姓名> <电话> [邮箱]");
            return 0;
        }
        ush_copy(entry.name, (u64)sizeof(entry.name), first);
        ush_copy(entry.phone, (u64)sizeof(entry.phone), second);
        if (third[0] != '\0') {
            ush_copy(entry.email, (u64)sizeof(entry.email), third);
        }
        contacts_format_line(&entry, line, (u64)sizeof(line));
        if (cmd[0] == 'a') {
            return contacts_append_entry(path, &entry);
        }
        return contacts_write_all(path, line);
    }

    ush_writeln_i18n("contacts: usage contacts [list|show|path|clear|add <name> <phone> [email]|remove <name>]",
                     "contacts: 用法 contacts [list|show|path|clear|add <姓名> <电话> [邮箱]|remove <姓名>]");
    return 0;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    char path[USH_PATH_MAX];
    int has_context = 0;
    int success;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
    contacts_default_path(path, (u64)sizeof(path));

    if (ush_command_ctx_read(&ctx) != 0 && ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "contacts") != 0) {
        has_context = 1;
        arg = ctx.arg;
        if (ctx.cwd[0] == '/') {
            ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
            ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
        }
    }

    success = contacts_handle(path, arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }
        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }
        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
