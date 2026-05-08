#include "cmd_runtime.h"

static const char *bootargs_loglevel_from_cmdline(const char *cmdline) {
    static char value[24];
    const char *keys[] = {"clks.loglevel=", "loglevel="};
    int key_index;

    value[0] = '\0';
    if (cmdline == (const char *)0) {
        return "";
    }

    for (key_index = 0; key_index < 2; key_index++) {
        const char *key = keys[key_index];
        u64 key_len = ush_strlen(key);
        u64 i = 0ULL;

        while (cmdline[i] != '\0') {
            u64 j = 0ULL;
            while (key[j] != '\0' && cmdline[i + j] == key[j]) {
                j++;
            }
            if (j == key_len) {
                u64 out = 0ULL;
                i += key_len;
                while (cmdline[i] != '\0' && cmdline[i] != ' ' && cmdline[i] != '\t') {
                    if (out + 1ULL < (u64)sizeof(value)) {
                        value[out++] = cmdline[i];
                    }
                    i++;
                }
                value[out] = '\0';
                return value;
            }
            i++;
        }
    }

    return "";
}

int cleonos_app_main(void) {
    cleonos_sysinfo info;
    char cmdline[512];
    char locale[CLEONOS_LOCALE_TEXT_MAX];
    char mount_path[USH_PATH_MAX];
    const char *loglevel;

    ush_zero(&info, (u64)sizeof(info));
    ush_zero(cmdline, (u64)sizeof(cmdline));
    ush_zero(locale, (u64)sizeof(locale));
    ush_zero(mount_path, (u64)sizeof(mount_path));

    (void)cleonos_sys_sysinfo(&info);
    (void)cleonos_sys_boot_cmdline(cmdline, (u64)sizeof(cmdline));
    (void)cleonos_sys_locale_get(locale, (u64)sizeof(locale));
    if (cleonos_sys_disk_mounted() != 0ULL) {
        (void)cleonos_sys_disk_mount_path(mount_path, (u64)sizeof(mount_path));
    }

    loglevel = bootargs_loglevel_from_cmdline(cmdline);

    ush_write_i18n_label("bootargs: cmdline", "bootargs: 启动参数");
    ush_write(" = ");
    ush_writeln(cmdline[0] != '\0' ? cmdline : "(empty)");

    ush_write_i18n_label("bootargs: boot mode", "bootargs: 启动模式");
    ush_write(" = ");
    ush_writeln(info.boot_mode[0] != '\0' ? info.boot_mode : "unknown");

    ush_write_i18n_label("bootargs: locale", "bootargs: 语言");
    ush_write(" = ");
    ush_writeln(locale[0] != '\0' ? locale : "unknown");

    ush_write_i18n_label("bootargs: loglevel", "bootargs: 日志等级");
    ush_write(" = ");
    ush_writeln(loglevel[0] != '\0' ? loglevel : "default");

    ush_write_i18n_label("bootargs: disk mounted", "bootargs: 硬盘挂载");
    ush_write(" = ");
    ush_writeln(cleonos_sys_disk_mounted() != 0ULL ? "yes" : "no");

    if (mount_path[0] != '\0') {
        ush_write_i18n_label("bootargs: disk mount path", "bootargs: 硬盘挂载点");
        ush_write(" = ");
        ush_writeln(mount_path);
    }

    return 0;
}
