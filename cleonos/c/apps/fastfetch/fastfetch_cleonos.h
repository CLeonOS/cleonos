#ifndef CLEONOS_FASTFETCH_CLEONOS_H
#define CLEONOS_FASTFETCH_CLEONOS_H

#include <cleonos_syscall.h>

#define FF_PORT_MAX_MODULES 32U
#define FF_PORT_TEXT_MAX 160U

typedef struct ff_port_config {
    int pipe;
    int no_logo;
    int json;
    int list_modules;
    int list_logos;
    int show_help;
    int show_version;
    int show_structure;
    const char *format;
    const char *logo;
    const char *structure_arg;
} ff_port_config;

typedef struct ff_port_context {
    ff_port_config config;
    cleonos_sysinfo sysinfo;
    cleonos_user_info user;
    char locale[CLEONOS_LOCALE_TEXT_MAX];
    char home[CLEONOS_USER_HOME_MAX];
    char shell[FF_PORT_TEXT_MAX];
    char os_pretty[FF_PORT_TEXT_MAX];
    char os_id[FF_PORT_TEXT_MAX];
    char os_version[FF_PORT_TEXT_MAX];
    char kernel_text[FF_PORT_TEXT_MAX];
    char uptime_text[FF_PORT_TEXT_MAX];
    char memory_text[FF_PORT_TEXT_MAX];
    char disk_text[FF_PORT_TEXT_MAX];
    char terminal_text[FF_PORT_TEXT_MAX];
    char resolution_text[FF_PORT_TEXT_MAX];
    char host_text[FF_PORT_TEXT_MAX];
    char network_text[FF_PORT_TEXT_MAX];
    char bootargs[FF_PORT_TEXT_MAX];
    const char *modules[FF_PORT_MAX_MODULES];
    unsigned int module_count;
} ff_port_context;

void ff_port_context_init(ff_port_context *ctx);
int ff_port_parse_args(ff_port_context *ctx, int argc, char **argv);
int ff_port_run(ff_port_context *ctx);

#endif
