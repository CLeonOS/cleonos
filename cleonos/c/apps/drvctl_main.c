#include "cmd_runtime.h"
#include <stdio.h>

static const char *drvctl_kind_name(u64 kind) {
    if (kind == 1ULL) {
        return "reserved";
    }
    if (kind == 2ULL) {
        return "elf";
    }
    return "unknown";
}

static const char *drvctl_state_name(u64 state) {
    if (state == 0ULL) {
        return "offline";
    }
    if (state == 1ULL) {
        return "ready";
    }
    if (state == 2ULL) {
        return "failed";
    }
    if (state == 3ULL) {
        return "loaded";
    }
    if (state == 4ULL) {
        return "unloaded";
    }
    return "unknown";
}

static const char *drvctl_class_name(u64 driver_class) {
    if (driver_class == 1ULL) {
        return "char";
    }
    if (driver_class == 2ULL) {
        return "video";
    }
    if (driver_class == 3ULL) {
        return "tty";
    }
    if (driver_class == 4ULL) {
        return "audio";
    }
    if (driver_class == 5ULL) {
        return "disk";
    }
    if (driver_class == 6ULL) {
        return "net";
    }
    if (driver_class == 7ULL) {
        return "input";
    }
    return "other";
}

static void drvctl_usage(void) {
    (void)puts("usage: drvctl list | load <path> | unload <name|path> | reload");
}

static int drvctl_list(void) {
    u64 count = cleonos_sys_driver_count();
    u64 i;

    (void)printf("drivers: %llu\n", (unsigned long long)count);

    for (i = 0ULL; i < count; i++) {
        cleonos_driver_info info;
        ush_zero(&info, (u64)sizeof(info));

        if (cleonos_sys_driver_info(i, &info, (u64)sizeof(info)) == 0ULL) {
            continue;
        }

        (void)printf("%llu: %-16s %-7s %-8s %-6s id=%llu size=%llu entry=0X%llX path=%s\n",
                     (unsigned long long)i, info.name, drvctl_kind_name(info.kind), drvctl_state_name(info.state),
                     drvctl_class_name(info.driver_class), (unsigned long long)info.load_id,
                     (unsigned long long)info.image_size, (unsigned long long)info.elf_entry,
                     info.path[0] != '\0' ? info.path : "-");
    }

    return 1;
}

static int drvctl_load(const char *path) {
    u64 id;

    if (path == (const char *)0 || path[0] == '\0') {
        drvctl_usage();
        return 0;
    }

    id = cleonos_sys_driver_load(path);
    if (id == 0ULL) {
        (void)puts("drvctl: load failed");
        return 0;
    }

    (void)printf("drvctl: loaded id=%llu\n", (unsigned long long)id);
    return 1;
}

static int drvctl_unload(const char *name_or_path) {
    if (name_or_path == (const char *)0 || name_or_path[0] == '\0') {
        drvctl_usage();
        return 0;
    }

    if (cleonos_sys_driver_unload(name_or_path) == 0ULL) {
        (void)puts("drvctl: unload failed");
        return 0;
    }

    (void)puts("drvctl: unloaded");
    return 1;
}

static int drvctl_reload(void) {
    u64 loaded = cleonos_sys_driver_reload();
    (void)printf("drvctl: reloaded %llu driver(s)\n", (unsigned long long)loaded);
    return 1;
}

static int drvctl_run(const char *arg) {
    char cmd[32];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == '\0') {
        return drvctl_list();
    }

    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
        drvctl_usage();
        return 0;
    }

    if (ush_streq(cmd, "list") != 0) {
        return drvctl_list();
    }
    if (ush_streq(cmd, "load") != 0) {
        return drvctl_load(rest);
    }
    if (ush_streq(cmd, "unload") != 0) {
        return drvctl_unload(rest);
    }
    if (ush_streq(cmd, "reload") != 0) {
        return drvctl_reload();
    }
    if (ush_streq(cmd, "--help") != 0 || ush_streq(cmd, "-h") != 0) {
        drvctl_usage();
        return 1;
    }

    drvctl_usage();
    return 0;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "drvctl") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = drvctl_run(arg);

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
