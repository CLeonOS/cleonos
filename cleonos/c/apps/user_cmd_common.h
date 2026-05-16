#ifndef CLEONOS_USER_CMD_COMMON_H
#define CLEONOS_USER_CMD_COMMON_H

#include "cmd_runtime.h"
#include "user/cleonos_user.h"

#if defined(__GNUC__) || defined(__clang__)
#define USH_UNUSED __attribute__((unused))
#else
#define USH_UNUSED
#endif

static void USH_UNUSED ucmd_load_context(ush_cmd_ctx *ctx, ush_state *sh, const char *expected_cmd, char *initial_cwd,
                                         int *out_has_context, const char **out_arg) {
    if (ctx == (ush_cmd_ctx *)0 || sh == (ush_state *)0 || expected_cmd == (const char *)0 ||
        initial_cwd == (char *)0 || out_has_context == (int *)0 || out_arg == (const char **)0) {
        return;
    }

    *out_has_context = 0;
    *out_arg = "";
    ush_copy(initial_cwd, (u64)USH_PATH_MAX, sh->cwd);

    if (ush_command_ctx_read(ctx) != 0 && ctx->cmd[0] != '\0' && ush_streq(ctx->cmd, expected_cmd) != 0) {
        *out_has_context = 1;
        *out_arg = ctx->arg;
        if (ctx->cwd[0] == '/') {
            ush_copy(sh->cwd, (u64)sizeof(sh->cwd), ctx->cwd);
            ush_copy(initial_cwd, (u64)USH_PATH_MAX, sh->cwd);
        }
    }
}

static void USH_UNUSED ucmd_finish_context(const ush_state *sh, const char *initial_cwd, int has_context) {
    ush_cmd_ret ret;

    if (sh == (const ush_state *)0 || initial_cwd == (const char *)0 || has_context == 0) {
        return;
    }

    ush_zero(&ret, (u64)sizeof(ret));

    if (ush_streq(sh->cwd, initial_cwd) == 0) {
        ret.flags |= USH_CMD_RET_FLAG_CWD;
        ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh->cwd);
    }

    if (sh->exit_requested != 0) {
        ret.flags |= USH_CMD_RET_FLAG_EXIT;
        ret.exit_code = sh->exit_code;
    }

    (void)ush_command_ret_write(&ret);
}

static int USH_UNUSED ucmd_read_line_internal(const char *prompt, char *out_line, u64 out_size, int secret) {
    u64 len = 0ULL;

    if (out_line == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_line[0] = '\0';
    if (prompt != (const char *)0 && prompt[0] != '\0') {
        ush_write(prompt);
    }

    for (;;) {
        int ch = getchar();

        if (ch == EOF) {
            out_line[len] = '\0';
            if (secret != 0) {
                ush_write_char('\n');
            }
            return (len > 0ULL) ? 1 : 0;
        }

        if (ch == '\r' || ch == '\n') {
            out_line[len] = '\0';
            ush_write_char('\n');
            return 1;
        }

        if (ch == 8 || ch == 127) {
            if (len > 0ULL) {
                len--;
                if (secret == 0) {
                    ush_write("\b \b");
                }
            }
            continue;
        }

        if (ush_is_printable((char)ch) == 0) {
            continue;
        }

        if (len + 1ULL >= out_size) {
            continue;
        }

        out_line[len++] = (char)ch;
        out_line[len] = '\0';

        if (secret == 0) {
            ush_write_char((char)ch);
        }
    }
}

static int USH_UNUSED ucmd_read_plain_line(const char *prompt, char *out_line, u64 out_size) {
    return ucmd_read_line_internal(prompt, out_line, out_size, 0);
}

static int USH_UNUSED ucmd_read_secret_line(const char *prompt, char *out_line, u64 out_size) {
    return ucmd_read_line_internal(prompt, out_line, out_size, 1);
}

static int USH_UNUSED ucmd_query_current_user(cleonos_user_info *out_info) {
    if (out_info == (cleonos_user_info *)0) {
        return 0;
    }

    (void)memset(out_info, 0, sizeof(*out_info));
    return (cleonos_sys_user_current(out_info) != 0ULL) ? 1 : 0;
}

static int USH_UNUSED ucmd_require_disk_accounts(const char *cmd, const cleonos_user_info *info) {
    if (cmd == (const char *)0 || info == (const cleonos_user_info *)0) {
        return 0;
    }

    if (info->disk_login_required == 0ULL) {
        ush_write(cmd);
        ush_writeln(": multi-user accounts are only enabled in disk boot mode");
        return 0;
    }

    if (info->logged_in == 0ULL) {
        ush_write(cmd);
        ush_writeln(": login required");
        return 0;
    }

    return 1;
}

static int USH_UNUSED ucmd_require_admin(const char *cmd, const cleonos_user_info *info) {
    if (ucmd_require_disk_accounts(cmd, info) == 0) {
        return 0;
    }

    if (info->role != CLEONOS_USER_ROLE_ADMIN) {
        ush_write(cmd);
        ush_writeln(": admin privileges required");
        return 0;
    }

    return 1;
}

static void USH_UNUSED ucmd_emit_user_info(const cleonos_user_info *info) {
    if (info == (const cleonos_user_info *)0) {
        return;
    }

    ush_write(info->name);
    ush_write("  ");
    ush_write((info->role == CLEONOS_USER_ROLE_ADMIN) ? "admin" : "user");
    ush_write("  ");
    ush_writeln(info->home);
}

#endif
