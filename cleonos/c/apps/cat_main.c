#include "cmd_runtime.h"
#include <stdio.h>

static int ush_cmd_cat(const ush_state *sh, const char *arg) {
    char path[USH_PATH_MAX];
    char buf[1024];
    u64 fd;

    if (arg == (const char *)0 || arg[0] == '\0') {
        if (ush_pipeline_stdin_text != (const char *)0) {
            (void)fputs(ush_pipeline_stdin_text, 1);
            return 1;
        }

        ush_writeln_i18n("cat: file path required", "cat: 需要文件路径");
        return 0;
    }

    if (ush_resolve_path(sh, arg, path, (u64)sizeof(path)) == 0) {
        ush_writeln_i18n("cat: invalid path", "cat: 无效路径");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(path) != 1ULL) {
        ush_writeln_i18n("cat: file not found", "cat: 文件不存在");
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        ush_writeln_i18n("cat: open failed", "cat: 打开失败");
        return 0;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
        u64 written_total = 0ULL;

        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln_i18n("cat: read failed", "cat: 读取失败");
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        while (written_total < got) {
            u64 written = cleonos_sys_fd_write(1ULL, buf + written_total, got - written_total);
            if (written == (u64)-1 || written == 0ULL) {
                (void)cleonos_sys_fd_close(fd);
                ush_writeln_i18n("cat: write failed", "cat: 写入失败");
                return 0;
            }
            written_total += written;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "cat") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_cat(&sh, arg);

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
