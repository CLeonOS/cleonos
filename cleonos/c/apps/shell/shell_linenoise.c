#include "shell_internal.h"

#define USH_LINENOISE_MAX_MATCHES 48ULL
#define USH_LINENOISE_TOKEN_MAX 96ULL
#define USH_LINENOISE_HINT_MAX 96ULL
#define USH_LINENOISE_DISPLAY_MAX 8ULL

static const char *const ush_linenoise_commands[] = {
    "help",       "args",       "ls",        "dir",       "cat",       "grep",     "head",
    "tail",       "wc",         "cut",       "uniq",      "sort",      "pwd",      "cd",
    "exec",       "run",        "clear",     "ansi",      "ansitest",  "color",    "bmpview",
    "qrcode",     "vim",        "uwm",       "wavplay",   "uname",     "sysinfo",  "fastfetch",
    "whoami",     "passwd",     "logout",    "users",     "useradd",   "userdel",  "usermod",
    "pkg",        "doom",       "memstat",   "fsstat",    "taskstat",  "userstat", "shstat",
    "stats",      "tty",        "dmesg",     "kbdstat",   "mkdir",     "touch",    "write",
    "append",     "cp",         "mv",        "rm",        "pid",       "spawn",    "bg",
    "wait",       "fg",         "kill",      "jobs",      "ps",        "procstat", "top",
    "sysstat",    "kdbg",       "sleep",     "yield",     "shutdown",  "restart",  "exit",
    "lua",        "calc",       "browser",   "wget",      "httpget",   "chat",     "imgview",
    "pngtest",    "zlibtest",   "zip",       "unzip",     "benchmark", "install2disk", "fsckfat32",
    "locale",     "chinese",
    (const char *)0,
};

typedef struct ush_linenoise_match_list {
    char items[USH_LINENOISE_MAX_MATCHES][USH_PATH_MAX];
    u64 count;
} ush_linenoise_match_list;

static int ush_linenoise_has_prefix(const char *text, const char *prefix) {
    u64 i = 0ULL;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static int ush_linenoise_contains(const char *text, const char *needle) {
    u64 i;
    u64 j;

    if (text == (const char *)0 || needle == (const char *)0) {
        return 0;
    }

    if (needle[0] == '\0') {
        return 1;
    }

    for (i = 0ULL; text[i] != '\0'; i++) {
        j = 0ULL;
        while (needle[j] != '\0' && text[i + j] != '\0' && text[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }

    return 0;
}

static void ush_linenoise_copy_n(char *dst, u64 dst_size, const char *src, u64 len) {
    u64 i;

    if (dst == (char *)0 || dst_size == 0ULL) {
        return;
    }

    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }

    if (len >= dst_size) {
        len = dst_size - 1ULL;
    }

    for (i = 0ULL; i < len; i++) {
        dst[i] = src[i];
    }
    dst[len] = '\0';
}

static void ush_linenoise_append(char *dst, u64 dst_size, const char *src) {
    u64 p = 0ULL;
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL || src == (const char *)0) {
        return;
    }

    while (dst[p] != '\0' && p + 1ULL < dst_size) {
        p++;
    }

    while (src[i] != '\0' && p + 1ULL < dst_size) {
        dst[p++] = src[i++];
    }
    dst[p] = '\0';
}

static const char *ush_linenoise_basename(const char *path) {
    const char *base = path;
    u64 i = 0ULL;

    if (path == (const char *)0) {
        return "";
    }

    while (path[i] != '\0') {
        if (path[i] == '/') {
            base = &path[i + 1ULL];
        }
        i++;
    }

    return base;
}

static int ush_linenoise_join_path(const char *dir, const char *name, char *out, u64 out_size) {
    u64 p = 0ULL;
    u64 i;

    if (dir == (const char *)0 || name == (const char *)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (dir[0] == '/' && dir[1] == '\0') {
        if (p + 1ULL >= out_size) {
            return 0;
        }
        out[p++] = '/';
    } else {
        for (i = 0ULL; dir[i] != '\0'; i++) {
            if (p + 1ULL >= out_size) {
                return 0;
            }
            out[p++] = dir[i];
        }
        if (p == 0ULL || out[p - 1ULL] != '/') {
            if (p + 1ULL >= out_size) {
                return 0;
            }
            out[p++] = '/';
        }
    }

    for (i = 0ULL; name[i] != '\0'; i++) {
        if (p + 1ULL >= out_size) {
            return 0;
        }
        out[p++] = name[i];
    }

    out[p] = '\0';
    return 1;
}

static void ush_linenoise_match_add(ush_linenoise_match_list *matches, const char *text) {
    if (matches == (ush_linenoise_match_list *)0 || text == (const char *)0 || text[0] == '\0') {
        return;
    }

    if (matches->count >= USH_LINENOISE_MAX_MATCHES) {
        return;
    }

    ush_copy(matches->items[matches->count], (u64)sizeof(matches->items[matches->count]), text);
    matches->count++;
}

static void ush_linenoise_complete_commands(const char *token, ush_linenoise_match_list *matches) {
    u64 i;

    if (matches == (ush_linenoise_match_list *)0 || token == (const char *)0) {
        return;
    }

    for (i = 0ULL; ush_linenoise_commands[i] != (const char *)0; i++) {
        if (ush_linenoise_has_prefix(ush_linenoise_commands[i], token) != 0) {
            ush_linenoise_match_add(matches, ush_linenoise_commands[i]);
        }
    }
}

static void ush_linenoise_complete_elf_dir(const char *dir, const char *token, ush_linenoise_match_list *matches) {
    u64 count;
    u64 i;

    if (dir == (const char *)0 || token == (const char *)0 || matches == (ush_linenoise_match_list *)0) {
        return;
    }

    if (cleonos_sys_fs_stat_type(dir) != 2ULL) {
        return;
    }

    count = cleonos_sys_fs_child_count(dir);
    for (i = 0ULL; i < count && matches->count < USH_LINENOISE_MAX_MATCHES; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char command[USH_PATH_MAX];
        u64 len;

        name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(dir, i, name) == 0ULL) {
            continue;
        }

        len = ush_strlen(name);
        if (len <= 4ULL || ush_streq(name + len - 4ULL, ".elf") == 0) {
            continue;
        }

        ush_linenoise_copy_n(command, (u64)sizeof(command), name, len - 4ULL);
        if (ush_linenoise_has_prefix(command, token) == 0) {
            continue;
        }

        ush_linenoise_match_add(matches, command);
    }
}

static void ush_linenoise_complete_external_commands(const char *token, ush_linenoise_match_list *matches) {
    ush_linenoise_complete_elf_dir("/shell", token, matches);
    ush_linenoise_complete_elf_dir("/shell/uwm", token, matches);
}

static int ush_linenoise_split_path_token(const ush_state *sh, const char *token, char *out_dir, u64 out_dir_size,
                                          char *out_prefix, u64 out_prefix_size, int *out_absolute) {
    u64 slash_pos = (u64)-1;
    u64 i;
    char parent[USH_PATH_MAX];

    if (sh == (const ush_state *)0 || token == (const char *)0 || out_dir == (char *)0 || out_prefix == (char *)0) {
        return 0;
    }

    for (i = 0ULL; token[i] != '\0'; i++) {
        if (token[i] == '/') {
            slash_pos = i;
        }
    }

    if (out_absolute != (int *)0) {
        *out_absolute = (token[0] == '/') ? 1 : 0;
    }

    if (slash_pos == (u64)-1) {
        ush_copy(out_dir, out_dir_size, sh->cwd);
        ush_copy(out_prefix, out_prefix_size, token);
        return 1;
    }

    if (slash_pos == 0ULL) {
        ush_copy(out_dir, out_dir_size, "/");
    } else {
        ush_linenoise_copy_n(parent, (u64)sizeof(parent), token, slash_pos);
        if (ush_resolve_path(sh, parent, out_dir, out_dir_size) == 0) {
            return 0;
        }
    }

    ush_copy(out_prefix, out_prefix_size, token + slash_pos + 1ULL);
    return 1;
}

static void ush_linenoise_complete_path(const ush_state *sh, const char *token, ush_linenoise_match_list *matches) {
    char dir[USH_PATH_MAX];
    char prefix[USH_PATH_MAX];
    char child_path[USH_PATH_MAX];
    u64 count;
    u64 i;

    if (sh == (const ush_state *)0 || token == (const char *)0 || matches == (ush_linenoise_match_list *)0) {
        return;
    }

    if (ush_linenoise_split_path_token(sh, token, dir, (u64)sizeof(dir), prefix, (u64)sizeof(prefix), (int *)0) == 0) {
        return;
    }

    if (cleonos_sys_fs_stat_type(dir) != 2ULL) {
        return;
    }

    count = cleonos_sys_fs_child_count(dir);
    for (i = 0ULL; i < count && matches->count < USH_LINENOISE_MAX_MATCHES; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char completion[USH_PATH_MAX];
        u64 type;

        name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(dir, i, name) == 0ULL) {
            continue;
        }

        if (ush_linenoise_has_prefix(name, prefix) == 0) {
            continue;
        }

        if (ush_linenoise_join_path(dir, name, child_path, (u64)sizeof(child_path)) == 0) {
            continue;
        }

        type = cleonos_sys_fs_stat_type(child_path);

        if (token[0] == '/') {
            ush_copy(completion, (u64)sizeof(completion), child_path);
        } else {
            const char *base = ush_linenoise_basename(token);
            u64 base_off = (u64)(base - token);
            ush_linenoise_copy_n(completion, (u64)sizeof(completion), token, base_off);
            ush_linenoise_append(completion, (u64)sizeof(completion), name);
        }

        if (type == 2ULL) {
            ush_linenoise_append(completion, (u64)sizeof(completion), "/");
        }

        ush_linenoise_match_add(matches, completion);
    }
}

static int ush_linenoise_find_token(const ush_state *sh, u64 *out_start, u64 *out_end, char *out_token, u64 out_token_size,
                                    int *out_command_token) {
    u64 start;
    u64 end;
    u64 i;
    int command_token = 1;

    if (sh == (const ush_state *)0 || out_start == (u64 *)0 || out_end == (u64 *)0 || out_token == (char *)0) {
        return 0;
    }

    end = sh->cursor;
    if (end > sh->line_len) {
        end = sh->line_len;
    }

    start = end;
    while (start > 0ULL && ush_is_space(sh->line[start - 1ULL]) == 0 && sh->line[start - 1ULL] != '|' &&
           sh->line[start - 1ULL] != '>' && sh->line[start - 1ULL] != '<') {
        start--;
    }

    for (i = 0ULL; i < start; i++) {
        if (sh->line[i] == '|' || sh->line[i] == ';') {
            command_token = 1;
        } else if (ush_is_space(sh->line[i]) == 0 && sh->line[i] != '>' && sh->line[i] != '<') {
            command_token = 0;
        }
    }

    *out_start = start;
    *out_end = end;
    ush_linenoise_copy_n(out_token, out_token_size, sh->line + start, end - start);

    if (out_command_token != (int *)0) {
        *out_command_token = command_token;
    }

    return 1;
}

static void ush_linenoise_replace_range(ush_state *sh, u64 start, u64 end, const char *replacement) {
    u64 repl_len;
    u64 tail_len;
    u64 i;

    if (sh == (ush_state *)0 || replacement == (const char *)0 || start > end || end > sh->line_len) {
        return;
    }

    repl_len = ush_strlen(replacement);
    tail_len = sh->line_len - end;

    if (start + repl_len + tail_len >= USH_LINE_MAX) {
        repl_len = (USH_LINE_MAX - 1ULL > start + tail_len) ? (USH_LINE_MAX - 1ULL - start - tail_len) : 0ULL;
    }

    for (i = 0ULL; i <= tail_len; i++) {
        sh->line[start + repl_len + i] = sh->line[end + i];
    }

    for (i = 0ULL; i < repl_len; i++) {
        sh->line[start + i] = replacement[i];
    }

    sh->line_len = start + repl_len + tail_len;
    sh->line[sh->line_len] = '\0';
    sh->cursor = start + repl_len;
}

static void ush_linenoise_common_prefix(const ush_linenoise_match_list *matches, char *out, u64 out_size) {
    u64 prefix_len;
    u64 i;
    u64 j;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (matches == (const ush_linenoise_match_list *)0 || matches->count == 0ULL) {
        return;
    }

    ush_copy(out, out_size, matches->items[0]);
    prefix_len = ush_strlen(out);

    for (i = 1ULL; i < matches->count; i++) {
        j = 0ULL;
        while (j < prefix_len && matches->items[i][j] != '\0' && out[j] == matches->items[i][j]) {
            j++;
        }
        prefix_len = j;
        out[prefix_len] = '\0';
    }
}

static void ush_linenoise_show_matches(const ush_linenoise_match_list *matches) {
    u64 i;
    u64 limit;

    if (matches == (const ush_linenoise_match_list *)0 || matches->count == 0ULL) {
        return;
    }

    limit = matches->count;
    if (limit > USH_LINENOISE_DISPLAY_MAX) {
        limit = USH_LINENOISE_DISPLAY_MAX;
    }

    ush_write_char('\n');
    for (i = 0ULL; i < limit; i++) {
        ush_write(matches->items[i]);
        if (i + 1ULL < limit) {
            ush_write("  ");
        }
    }
    if (matches->count > limit) {
        ush_write("  ...");
    }
    ush_write_char('\n');
}

u64 ush_linenoise_hint_visible_len(const char *hint) {
    return ush_strlen(hint);
}

const char *ush_linenoise_hint(const ush_state *sh) {
    static char hint[USH_LINENOISE_HINT_MAX];
    char cmd[USH_CMD_MAX];
    char arg[USH_ARG_MAX];

    hint[0] = '\0';
    if (sh == (const ush_state *)0 || sh->line_len == 0ULL || sh->cursor != sh->line_len) {
        return hint;
    }

    ush_parse_line(sh->line, cmd, (u64)sizeof(cmd), arg, (u64)sizeof(arg));

    if (cmd[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), "help");
    } else if (ush_streq(cmd, "pkg") != 0 && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " install | list | search | update | upgrade");
    } else if (ush_streq(cmd, "chat") != 0 && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " server | register | login | msg");
    } else if (ush_streq(cmd, "cd") != 0 && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " /system /shell /home /temp");
    } else if ((ush_streq(cmd, "exec") != 0 || ush_streq(cmd, "run") != 0) && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " /shell/<app>.elf");
    } else if (ush_streq(cmd, "browser") != 0 && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " http://example.com");
    } else if (ush_streq(cmd, "wget") != 0 && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " <url> [-o file]");
    } else if (ush_streq(cmd, "lua") != 0 && arg[0] == '\0') {
        ush_copy(hint, (u64)sizeof(hint), " [script.lua]");
    } else {
        u64 i;
        for (i = 0ULL; ush_linenoise_commands[i] != (const char *)0; i++) {
            if (ush_linenoise_has_prefix(ush_linenoise_commands[i], cmd) != 0 && ush_streq(ush_linenoise_commands[i], cmd) == 0) {
                ush_copy(hint, (u64)sizeof(hint), ush_linenoise_commands[i] + ush_strlen(cmd));
                break;
            }
        }
    }

    return hint;
}

int ush_linenoise_complete(ush_state *sh) {
    ush_linenoise_match_list matches;
    char token[USH_LINENOISE_TOKEN_MAX];
    char common[USH_PATH_MAX];
    u64 start;
    u64 end;
    int command_token = 0;

    if (sh == (ush_state *)0) {
        return USH_LINENOISE_COMPLETE_NONE;
    }

    (void)memset(&matches, 0, sizeof(matches));
    if (ush_linenoise_find_token(sh, &start, &end, token, (u64)sizeof(token), &command_token) == 0) {
        return USH_LINENOISE_COMPLETE_NONE;
    }

    if (command_token != 0 && ush_contains_char(token, '/') == 0) {
        ush_linenoise_complete_commands(token, &matches);
        ush_linenoise_complete_external_commands(token, &matches);
    }
    ush_linenoise_complete_path(sh, token, &matches);

    if (matches.count == 0ULL) {
        ush_write("\a");
        return USH_LINENOISE_COMPLETE_NONE;
    }

    if (matches.count == 1ULL) {
        ush_linenoise_replace_range(sh, start, end, matches.items[0]);
        if (sh->line_len + 1ULL < USH_LINE_MAX && matches.items[0][ush_strlen(matches.items[0]) - 1ULL] != '/') {
            ush_linenoise_replace_range(sh, sh->cursor, sh->cursor, " ");
        }
        ush_input_render_line(sh);
        return USH_LINENOISE_COMPLETE_EDITED;
    }

    ush_linenoise_common_prefix(&matches, common, (u64)sizeof(common));
    if (ush_strlen(common) > ush_strlen(token)) {
        ush_linenoise_replace_range(sh, start, end, common);
        ush_input_render_line(sh);
        return USH_LINENOISE_COMPLETE_EDITED;
    }

    ush_linenoise_show_matches(&matches);
    ush_input_render_line(sh);
    return USH_LINENOISE_COMPLETE_LISTED;
}

void ush_linenoise_reverse_search(ush_state *sh) {
    char query[USH_LINE_MAX];
    u64 query_len = 0ULL;

    if (sh == (ush_state *)0) {
        return;
    }

    query[0] = '\0';
    ush_write("\n(reverse-i-search)`': ");

    for (;;) {
        char ch = ush_input_read_char_blocking();
        i64 i;
        const char *match = (const char *)0;

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            ush_write_char('\n');
            if (query_len > 0ULL) {
                for (i = (i64)sh->history_count - 1; i >= 0; i--) {
                    if (ush_linenoise_contains(sh->history[(u64)i], query) != 0) {
                        match = sh->history[(u64)i];
                        break;
                    }
                }
            }
            if (match != (const char *)0) {
                ush_copy(sh->line, (u64)sizeof(sh->line), match);
                sh->line_len = ush_strlen(sh->line);
                sh->cursor = sh->line_len;
            }
            ush_input_render_line(sh);
            return;
        }

        if (ch == 27 || ch == USH_KEY_REVERSE_SEARCH) {
            ush_write_char('\n');
            ush_input_render_line(sh);
            return;
        }

        if (ch == '\b' || ch == 127 || ch == USH_KEY_DELETE) {
            if (query_len > 0ULL) {
                query_len--;
                query[query_len] = '\0';
            }
        } else if (ush_is_printable(ch) != 0 && query_len + 1ULL < (u64)sizeof(query)) {
            query[query_len++] = ch;
            query[query_len] = '\0';
        } else {
            continue;
        }

        for (i = (i64)sh->history_count - 1; i >= 0; i--) {
            if (ush_linenoise_contains(sh->history[(u64)i], query) != 0) {
                match = sh->history[(u64)i];
                break;
            }
        }

        ush_write("\r(reverse-i-search)`");
        ush_write(query);
        ush_write("': ");
        ush_write((match != (const char *)0) ? match : "");
        ush_write("          ");
    }
}
