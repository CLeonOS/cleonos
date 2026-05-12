#include "fastfetch_cleonos.h"

#include <stdio.h>
#include <string.h>

int ff_port_streq(const char *left, const char *right);
void ff_port_json_escape_print(const char *text);

typedef const char *(*ff_module_value_fn)(const ff_port_context *ctx, char *scratch, unsigned long scratch_size);

typedef struct ff_module_def {
    const char *name;
    ff_module_value_fn value;
} ff_module_def;

static const char *ff_mod_title(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)snprintf(scratch, scratch_size, "%s@%s", ctx->user.name[0] ? ctx->user.name : "user", ctx->os_id);
    return scratch;
}

static const char *ff_mod_separator(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)ctx;
    (void)scratch_size;
    (void)strcpy(scratch, "----------------");
    return scratch;
}

static const char *ff_mod_os(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)snprintf(scratch, scratch_size, "%s %s", ctx->os_pretty, ctx->os_version);
    return scratch;
}

static const char *ff_mod_host(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->host_text;
}

static const char *ff_mod_kernel(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->kernel_text;
}

static const char *ff_mod_uptime(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->uptime_text;
}

static const char *ff_mod_shell(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->shell;
}

static const char *ff_mod_terminal(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->terminal_text;
}

static const char *ff_mod_terminal_size(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->resolution_text;
}

static const char *ff_mod_memory(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->memory_text;
}

static const char *ff_mod_disk(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->disk_text;
}

static const char *ff_mod_locale(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->locale;
}

static const char *ff_mod_user(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)snprintf(scratch, scratch_size, "%s (%s)", ctx->user.name[0] ? ctx->user.name : "user",
                   ctx->user.role == CLEONOS_USER_ROLE_ADMIN ? "admin" : "user");
    return scratch;
}

static const char *ff_mod_home(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->home;
}

static const char *ff_mod_network(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)scratch;
    (void)scratch_size;
    return ctx->network_text;
}

static const char *ff_mod_bootmgr(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)snprintf(scratch, scratch_size, "%s boot%s%s", ctx->sysinfo.boot_mode[0] ? ctx->sysinfo.boot_mode : "unknown",
                   ctx->bootargs[0] ? ", cmdline: " : "", ctx->bootargs[0] ? ctx->bootargs : "");
    return scratch;
}

static const char *ff_mod_processes(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)snprintf(scratch, scratch_size, "%llu tasks, %llu services (%llu ready)",
                   (unsigned long long)ctx->sysinfo.task_count, (unsigned long long)ctx->sysinfo.service_count,
                   (unsigned long long)ctx->sysinfo.service_ready_count);
    return scratch;
}

static const char *ff_mod_packages(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)ctx;
    (void)snprintf(scratch, scratch_size, "pkg db: %s",
                   cleonos_sys_fs_stat_type("/system/pkg/installed.db") != 0ULL ? "/system/pkg/installed.db"
                                                                                 : "not initialized");
    return scratch;
}

static const char *ff_mod_colors(const ff_port_context *ctx, char *scratch, unsigned long scratch_size) {
    (void)ctx;
    (void)scratch;
    (void)scratch_size;
    return "\x1B[40m  \x1B[41m  \x1B[42m  \x1B[43m  \x1B[44m  \x1B[45m  \x1B[46m  \x1B[47m  \x1B[0m";
}

static const ff_module_def ff_modules[] = {
    {"title", ff_mod_title},       {"separator", ff_mod_separator}, {"os", ff_mod_os},
    {"host", ff_mod_host},         {"kernel", ff_mod_kernel},       {"uptime", ff_mod_uptime},
    {"shell", ff_mod_shell},       {"terminal", ff_mod_terminal},   {"terminalsize", ff_mod_terminal_size},
    {"memory", ff_mod_memory},     {"disk", ff_mod_disk},           {"locale", ff_mod_locale},
    {"user", ff_mod_user},         {"home", ff_mod_home},           {"network", ff_mod_network},
    {"bootmgr", ff_mod_bootmgr},   {"processes", ff_mod_processes}, {"packages", ff_mod_packages},
    {"colors", ff_mod_colors},
};

static const unsigned int ff_module_count = (unsigned int)(sizeof(ff_modules) / sizeof(ff_modules[0]));

const ff_module_def *ff_port_find_module(const char *name) {
    unsigned int i;

    for (i = 0U; i < ff_module_count; i++) {
        if (ff_port_streq(name, ff_modules[i].name) != 0) {
            return &ff_modules[i];
        }
    }
    return (const ff_module_def *)0;
}

void ff_port_add_default_modules(ff_port_context *ctx) {
    static const char *defaults[] = {"title",  "separator", "os",     "host",   "kernel", "uptime", "packages",
                                     "shell", "terminal",  "display", "memory", "disk",   "locale", "colors"};
    unsigned int i;

    ctx->module_count = 0U;
    for (i = 0U; i < (unsigned int)(sizeof(defaults) / sizeof(defaults[0])); i++) {
        const char *name = defaults[i];
        if (ff_port_streq(name, "display") != 0) {
            name = "terminalsize";
        }
        ctx->modules[ctx->module_count++] = name;
    }
}

void ff_port_list_modules(void) {
    unsigned int i;

    for (i = 0U; i < ff_module_count; i++) {
        (void)puts(ff_modules[i].name);
    }
}

static void ff_port_print_line(const ff_module_def *module, const char *value, int pipe) {
    if (ff_port_streq(module->name, "title") != 0 || ff_port_streq(module->name, "separator") != 0) {
        (void)puts(value);
        return;
    }
    if (pipe == 0) {
        (void)printf("\x1B[1;36m%-13s\x1B[0m %s\n", module->name, value);
    } else {
        (void)printf("%-13s %s\n", module->name, value);
    }
}

void ff_port_print_modules(const ff_port_context *ctx) {
    unsigned int i;
    char scratch[FF_PORT_TEXT_MAX];

    for (i = 0U; i < ctx->module_count; i++) {
        const ff_module_def *module = ff_port_find_module(ctx->modules[i]);
        const char *value;

        if (module == (const ff_module_def *)0) {
            if (ctx->config.pipe == 0) {
                (void)printf("\x1B[1;31m%-13s\x1B[0m unsupported\n", ctx->modules[i]);
            } else {
                (void)printf("%-13s unsupported\n", ctx->modules[i]);
            }
            continue;
        }

        scratch[0] = '\0';
        value = module->value(ctx, scratch, sizeof(scratch));
        ff_port_print_line(module, value, ctx->config.pipe);
    }
}

void ff_port_print_json(const ff_port_context *ctx) {
    unsigned int i;
    int first = 1;
    char scratch[FF_PORT_TEXT_MAX];

    (void)puts("[");
    for (i = 0U; i < ctx->module_count; i++) {
        const ff_module_def *module = ff_port_find_module(ctx->modules[i]);
        const char *value;

        if (module == (const ff_module_def *)0) {
            continue;
        }
        scratch[0] = '\0';
        value = module->value(ctx, scratch, sizeof(scratch));
        (void)printf("%s  { \"type\": \"", first ? "" : ",\n");
        ff_port_json_escape_print(module->name);
        (void)printf("\", \"result\": \"");
        ff_port_json_escape_print(value);
        (void)printf("\" }");
        first = 0;
    }
    (void)puts("\n]");
}
