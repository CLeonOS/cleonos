#include "user_cmd_common.h"
#include <stdio.h>

#define TODO_LINE_MAX 512U

static void todo_default_path(char *out_path, u64 out_size) {
    cleonos_user_info info;

    if (out_path == (char *)0 || out_size == 0ULL) {
        return;
    }

    if (cleonos_sys_user_current(&info) != 0ULL && info.logged_in != 0ULL && info.home[0] != '\0') {
        (void)snprintf(out_path, (size_t)out_size, "%s/todo.txt", info.home);
        return;
    }

    ush_copy(out_path, out_size, "/temp/todo.txt");
}

static int todo_read_all(const char *path, char *out, u64 out_size, u64 *out_len) {
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

static int todo_write_all(const char *path, const char *data) {
    return (cleonos_sys_fs_write(path, data, ush_strlen(data)) != 0ULL) ? 1 : 0;
}

static int todo_print_items(const char *path) {
    static char db[8192];
    u64 len = 0ULL;
    u64 start = 0ULL;
    size_t index = 0U;
    u64 i;

    if (todo_read_all(path, db, (u64)sizeof(db), &len) == 0) {
        ush_writeln_i18n("todo: read failed", "todo: 读取失败");
        return 0;
    }

    if (len == 0ULL) {
        ush_writeln_i18n("todo: empty", "todo: 为空");
        return 1;
    }

    for (i = 0ULL; i <= len; i++) {
        if (i == len || db[i] == '\n') {
            char line[TODO_LINE_MAX];
            u64 line_len = i - start;
            if (line_len >= (u64)sizeof(line)) {
                line_len = (u64)sizeof(line) - 1ULL;
            }
            (void)memcpy(line, db + start, (size_t)line_len);
            line[line_len] = '\0';
            if (line[0] != '\0') {
                char idx[16];
                (void)snprintf(idx, sizeof(idx), "%zu", index + 1U);
                ush_write("[");
                ush_write(idx);
                ush_write("] ");
                ush_writeln(line);
                index++;
            }
            start = i + 1ULL;
        }
    }

    return 1;
}

static int todo_add_item(const char *path, const char *text) {
    char line[TODO_LINE_MAX];

    if (text == (const char *)0 || text[0] == '\0') {
        ush_writeln_i18n("todo: usage todo add <text>", "todo: 用法 todo add <文本>");
        return 0;
    }

    (void)snprintf(line, sizeof(line), "- %s\n", text);
    return (cleonos_sys_fs_append(path, line, ush_strlen(line)) != 0ULL) ? 1 : 0;
}

static int todo_remove_item(const char *path, const char *arg) {
    char db[8192];
    char next_db[8192];
    char num_buf[32];
    u64 len = 0ULL;
    u64 start = 0ULL;
    size_t index = 0U;
    u64 target = 0ULL;
    u64 i;

    if (ush_parse_u64_dec(arg, &target) == 0 || target == 0ULL) {
        ush_writeln_i18n("todo: usage todo remove <number>", "todo: 用法 todo remove <编号>");
        return 0;
    }

    if (todo_read_all(path, db, (u64)sizeof(db), &len) == 0) {
        return 0;
    }

    next_db[0] = '\0';
    for (i = 0ULL; i <= len; i++) {
        if (i == len || db[i] == '\n') {
            char line[TODO_LINE_MAX];
            u64 line_len = i - start;

            if (line_len >= (u64)sizeof(line)) {
                line_len = (u64)sizeof(line) - 1ULL;
            }
            (void)memcpy(line, db + start, (size_t)line_len);
            line[line_len] = '\0';
            if (line[0] != '\0') {
                index++;
                if ((u64)index != target) {
                    (void)snprintf(num_buf, sizeof(num_buf), "%s\n", line);
                    ush_copy(next_db + ush_strlen(next_db), (u64)(sizeof(next_db) - ush_strlen(next_db)), num_buf);
                }
            }
            start = i + 1ULL;
        }
    }

    return todo_write_all(path, next_db);
}

static int todo_handle(const char *path, const char *arg) {
    char cmd[32];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
        return todo_print_items(path);
    }

    if (ush_streq(cmd, "list") != 0 || ush_streq(cmd, "show") != 0) {
        return todo_print_items(path);
    }

    if (ush_streq(cmd, "path") != 0) {
        ush_writeln(path);
        return 1;
    }

    if (ush_streq(cmd, "clear") != 0) {
        return todo_write_all(path, "");
    }

    if (ush_streq(cmd, "add") != 0 || ush_streq(cmd, "set") != 0) {
        return todo_add_item(path, rest);
    }

    if (ush_streq(cmd, "remove") != 0 || ush_streq(cmd, "rm") != 0) {
        return todo_remove_item(path, rest);
    }

    ush_writeln_i18n("todo: usage todo [list|show|path|clear|add <text>|remove <number>]",
                     "todo: 用法 todo [list|show|path|clear|add <文本>|remove <编号>]");
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
    todo_default_path(path, (u64)sizeof(path));

    if (ush_command_ctx_read(&ctx) != 0 && ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "todo") != 0) {
        has_context = 1;
        arg = ctx.arg;
        if (ctx.cwd[0] == '/') {
            ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
            ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
        }
    }

    success = todo_handle(path, arg);

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
