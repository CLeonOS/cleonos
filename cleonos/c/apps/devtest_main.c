#include "cmd_runtime.h"
#include <stdio.h>

static int devtest_read_once(const char *path) {
    char buf[256];
    u64 fd;
    u64 got;

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        (void)printf("%s: open failed\n", path);
        return 0;
    }

    got = cleonos_sys_fd_read(fd, buf, (u64)(sizeof(buf) - 1U));
    if (got == (u64)-1) {
        (void)cleonos_sys_fd_close(fd);
        (void)printf("%s: read failed\n", path);
        return 0;
    }

    buf[got] = '\0';
    (void)printf("%s: %s", path, buf);
    if (got == 0ULL || buf[got - 1ULL] != '\n') {
        (void)putchar('\n');
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

static int devtest_fb_clear(const char *arg) {
    u64 fd;
    const char *color = "clear 202020";

    if (arg != (const char *)0 && arg[0] != '\0') {
        color = arg;
    }

    fd = cleonos_sys_fd_open("/dev/fb0", CLEONOS_O_WRONLY, 0ULL);
    if (fd == (u64)-1) {
        (void)puts("/dev/fb0: open write failed");
        return 0;
    }

    if (cleonos_sys_fd_write(fd, color, (u64)ush_strlen(color)) == (u64)-1) {
        (void)cleonos_sys_fd_close(fd);
        (void)puts("/dev/fb0: write failed");
        return 0;
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

static int devtest_run(const char *arg) {
    char cmd[32];
    const char *rest = "";
    int ok = 1;

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
            return 0;
        }

        if (ush_streq(cmd, "clearfb") != 0) {
            return devtest_fb_clear(rest);
        }

        return devtest_read_once(arg);
    }

    ok &= devtest_read_once("/dev/fb0");
    ok &= devtest_read_once("/dev/net0");
    ok &= devtest_read_once("/dev/disk0");
    ok &= devtest_read_once("/dev/input/mouse");
    (void)puts("/dev/input/kbd: non-blocking char stream; read it manually with cat if needed");
    return ok;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "devtest") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = devtest_run(arg);

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
