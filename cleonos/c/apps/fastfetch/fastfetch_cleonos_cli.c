#include "fastfetch_cleonos.h"

#include <stdio.h>
#include <string.h>

int ff_port_streq(const char *left, const char *right);
int ff_port_starts_with(const char *text, const char *prefix);
void ff_port_copy(char *dst, unsigned long dst_size, const char *src);
void ff_port_add_default_modules(ff_port_context *ctx);
void ff_port_list_modules(void);

static void ff_port_structure_set(ff_port_context *ctx, const char *structure) {
    const char *p = structure;

    ctx->module_count = 0U;
    while (p != (const char *)0 && *p != '\0' && ctx->module_count < FF_PORT_MAX_MODULES) {
        char token[32];
        unsigned long at = 0UL;

        while (*p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        while (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t' && at + 1UL < sizeof(token)) {
            token[at++] = *p++;
        }
        token[at] = '\0';
        if (token[0] != '\0') {
            static char module_storage[FF_PORT_MAX_MODULES][32];
            ff_port_copy(module_storage[ctx->module_count], sizeof(module_storage[ctx->module_count]), token);
            ctx->modules[ctx->module_count] = module_storage[ctx->module_count];
            ctx->module_count++;
        }
    }

    if (ctx->module_count == 0U) {
        ff_port_add_default_modules(ctx);
    }
}

static void ff_port_print_help(void) {
    (void)puts("Fastfetch CLeonOS port");
    (void)puts("");
    (void)puts("Usage: fastfetch [options]");
    (void)puts("");
    (void)puts("Options:");
    (void)puts("  -h, --help                 Show help");
    (void)puts("  -v, --version              Show upstream fastfetch version");
    (void)puts("      --pipe                 Disable colors");
    (void)puts("      --json                 Print JSON result");
    (void)puts("      --list-modules         List supported modules");
    (void)puts("      --list-logos           List supported logos");
    (void)puts("      --logo <name>          Select logo (cleonos, none)");
    (void)puts("      --structure <list>     Comma separated modules");
    (void)puts("");
    (void)puts("This port keeps upstream fastfetch in third-party/fastfetch and provides a CLeonOS platform backend.");
}

int ff_port_parse_args(ff_port_context *ctx, int argc, char **argv) {
    int i;

    ff_port_add_default_modules(ctx);
    for (i = 1; i < argc; i++) {
        const char *arg = (argv != (char **)0) ? argv[i] : (const char *)0;

        if (arg == (const char *)0) {
            continue;
        }
        if (ff_port_streq(arg, "-h") != 0 || ff_port_streq(arg, "--help") != 0) {
            ctx->config.show_help = 1;
        } else if (ff_port_streq(arg, "-v") != 0 || ff_port_streq(arg, "--version") != 0) {
            ctx->config.show_version = 1;
        } else if (ff_port_streq(arg, "--pipe") != 0 || ff_port_streq(arg, "--plain") != 0) {
            ctx->config.pipe = 1;
        } else if (ff_port_streq(arg, "--json") != 0 || ff_port_streq(arg, "--format json") != 0) {
            ctx->config.json = 1;
            ctx->config.pipe = 1;
        } else if (ff_port_streq(arg, "--no-logo") != 0) {
            ctx->config.no_logo = 1;
        } else if (ff_port_streq(arg, "--list-modules") != 0) {
            ctx->config.list_modules = 1;
        } else if (ff_port_streq(arg, "--list-logos") != 0 || ff_port_streq(arg, "--print-logos") != 0) {
            ctx->config.list_logos = 1;
        } else if (ff_port_streq(arg, "--logo") != 0 && i + 1 < argc) {
            ctx->config.logo = argv[++i];
            if (ff_port_streq(ctx->config.logo, "none") != 0) {
                ctx->config.no_logo = 1;
            }
        } else if (ff_port_starts_with(arg, "--logo=") != 0) {
            ctx->config.logo = arg + strlen("--logo=");
            if (ff_port_streq(ctx->config.logo, "none") != 0) {
                ctx->config.no_logo = 1;
            }
        } else if (ff_port_streq(arg, "--structure") != 0 && i + 1 < argc) {
            ctx->config.structure_arg = argv[++i];
            ff_port_structure_set(ctx, ctx->config.structure_arg);
        } else if (ff_port_starts_with(arg, "--structure=") != 0) {
            ctx->config.structure_arg = arg + strlen("--structure=");
            ff_port_structure_set(ctx, ctx->config.structure_arg);
        } else {
            (void)printf("fastfetch: unsupported option: %s\n", arg);
            return 0;
        }
    }
    return 1;
}

int ff_port_run(ff_port_context *ctx) {
    void ff_port_print_logo(const ff_port_context *ctx);
    void ff_port_print_modules(const ff_port_context *ctx);
    void ff_port_print_json(const ff_port_context *ctx);

    if (ctx->config.show_help != 0) {
        ff_port_print_help();
        return 1;
    }
    if (ctx->config.show_version != 0) {
        (void)puts("fastfetch 2.62.1 (CLeonOS port)");
        return 1;
    }
    if (ctx->config.list_modules != 0) {
        ff_port_list_modules();
        return 1;
    }
    if (ctx->config.list_logos != 0) {
        (void)puts("cleonos");
        (void)puts("none");
        return 1;
    }
    if (ctx->config.json != 0) {
        ff_port_print_json(ctx);
        return 1;
    }
    if (ctx->config.no_logo == 0) {
        ff_port_print_logo(ctx);
    }
    ff_port_print_modules(ctx);
    return 1;
}
