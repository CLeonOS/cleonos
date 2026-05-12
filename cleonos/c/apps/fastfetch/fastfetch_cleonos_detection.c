#include "fastfetch_cleonos_compat.h"

#include <cleonos_syscall.h>

#include "common/io.h"
#include "common/FFstrbuf.h"
#include "detection/disk/disk.h"
#include "detection/host/host.h"
#include "detection/locale/locale.h"
#include "detection/memory/memory.h"
#include "detection/os/os.h"
#include "detection/libc/libc.h"
#include "detection/terminalshell/terminalshell.h"
#include "detection/terminalsize/terminalsize.h"
#include "detection/uptime/uptime.h"

#include <string.h>

static FFOSResult g_os;
static int g_os_inited;

static void ff_cl_parse_kv(const char *data, const char *key, FFstrbuf *out) {
    size_t key_len = strlen(key);
    const char *p = data;

    while (p != (const char *)0 && *p != '\0') {
        const char *line = p;
        size_t len = 0U;
        while (p[len] != '\0' && p[len] != '\n' && p[len] != '\r') {
            len++;
        }
        if (len > key_len && strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            const char *value = line + key_len + 1U;
            if (*value == '"') {
                value++;
                len = 0U;
                while (value[len] != '\0' && value[len] != '"' && value[len] != '\n' && value[len] != '\r') {
                    len++;
                }
            } else {
                len = 0U;
                while (value[len] != '\0' && value[len] != '\n' && value[len] != '\r') {
                    len++;
                }
            }
            ffStrbufSetNS(out, (uint32_t)len, value);
            return;
        }
        p += len;
        while (*p == '\n' || *p == '\r') {
            p++;
        }
    }
}

const FFOSResult *ffDetectOS(void) {
    char data[1024];

    if (g_os_inited != 0) {
        return &g_os;
    }
    g_os_inited = 1;
    ffStrbufInit(&g_os.name);
    ffStrbufInit(&g_os.prettyName);
    ffStrbufInit(&g_os.id);
    ffStrbufInit(&g_os.idLike);
    ffStrbufInit(&g_os.variant);
    ffStrbufInit(&g_os.variantID);
    ffStrbufInit(&g_os.version);
    ffStrbufInit(&g_os.versionID);
    ffStrbufInit(&g_os.codename);
    ffStrbufInit(&g_os.buildID);

    ffStrbufSetS(&g_os.name, "CLeonOS");
    ffStrbufSetS(&g_os.prettyName, "CLeonOS");
    ffStrbufSetS(&g_os.id, "cleonos");
    ffStrbufSetS(&g_os.versionID, "0.1");

    data[0] = '\0';
    if (cleonos_sys_fs_read("/etc/os-version", data, (u64)sizeof(data) - 1ULL) == 0ULL) {
        (void)cleonos_sys_fs_read("/etc/os-release", data, (u64)sizeof(data) - 1ULL);
    }
    data[sizeof(data) - 1U] = '\0';
    ff_cl_parse_kv(data, "NAME", &g_os.name);
    ff_cl_parse_kv(data, "PRETTY_NAME", &g_os.prettyName);
    ff_cl_parse_kv(data, "ID", &g_os.id);
    ff_cl_parse_kv(data, "VERSION_ID", &g_os.versionID);
    ff_cl_parse_kv(data, "VERSION_CODENAME", &g_os.codename);
    return &g_os;
}

const char *ffDetectUptime(FFUptimeResult *result) {
    cleonos_sysinfo info;
    if (result == (FFUptimeResult *)0) {
        return "invalid result";
    }
    (void)memset(&info, 0, sizeof(info));
    (void)cleonos_sys_sysinfo(&info);
    result->uptime = info.uptime_ms;
    result->bootTime = cleonos_sys_time_ms() > info.uptime_ms ? cleonos_sys_time_ms() - info.uptime_ms : 0ULL;
    return (const char *)0;
}

const char *ffDetectMemory(FFMemoryResult *ram) {
    cleonos_sysinfo info;
    if (ram == (FFMemoryResult *)0) {
        return "invalid result";
    }
    (void)memset(&info, 0, sizeof(info));
    (void)cleonos_sys_sysinfo(&info);
    ram->bytesTotal = info.managed_pages * 4096ULL;
    ram->bytesUsed = info.used_pages * 4096ULL;
    return (const char *)0;
}

const FFShellResult *ffDetectShell(void) {
    static FFShellResult result;
    static int inited;
    if (inited == 0) {
        inited = 1;
        ffStrbufInitS(&result.processName, "shell");
        ffStrbufInitS(&result.exe, "/shell/shell.elf");
        result.exeName = "shell.elf";
        ffStrbufInitS(&result.exePath, "/shell/shell.elf");
        ffStrbufInitS(&result.prettyName, "CLeonOS Shell");
        ffStrbufInitS(&result.version, "");
        result.pid = (uint32_t)cleonos_sys_getpid();
        result.ppid = 0;
        result.tty = (int32_t)cleonos_sys_tty_active();
    }
    return &result;
}

const FFTerminalResult *ffDetectTerminal(void) {
    static FFTerminalResult terminal;
    static int inited;
    if (inited == 0) {
        inited = 1;
        ffStrbufInitS(&terminal.processName, "tty");
        ffStrbufInitS(&terminal.exe, "/dev/tty0");
        terminal.exeName = "tty";
        ffStrbufInitS(&terminal.exePath, "/dev/tty0");
        ffStrbufInitS(&terminal.prettyName, "CLeonOS TTY");
        ffStrbufInitS(&terminal.version, "");
        ffStrbufInitF(&terminal.tty, "tty%llu", (unsigned long long)cleonos_sys_tty_active());
        terminal.pid = 0;
        terminal.ppid = 0;
    }
    return &terminal;
}

bool ffDetectTerminalSize(FFTerminalSizeResult *result) {
    cleonos_tty_grid_info grid;
    if (result == (FFTerminalSizeResult *)0) {
        return false;
    }
    (void)memset(&grid, 0, sizeof(grid));
    (void)cleonos_sys_tty_grid_info(&grid);
    result->columns = (uint32_t)grid.cols;
    result->rows = (uint32_t)grid.rows;
    result->width = 0;
    result->height = 0;
    return true;
}

const char *ffDetectLocale(FFstrbuf *result) {
    char locale[CLEONOS_LOCALE_TEXT_MAX];
    if (cleonos_sys_locale_get(locale, (u64)sizeof(locale)) == 0ULL || locale[0] == '\0') {
        ffStrbufSetS(result, "en_US");
    } else {
        ffStrbufSetS(result, locale);
    }
    return (const char *)0;
}

const char *ffDetectHost(FFHostResult *host) {
    if (host == (FFHostResult *)0) {
        return "invalid result";
    }
    ffStrbufSetS(&host->family, "CLeonOS");
    ffStrbufSetS(&host->name, "CLeonOS Machine");
    ffStrbufSetS(&host->version, "virtual");
    ffStrbufSetS(&host->sku, "cleonos");
    ffStrbufSetS(&host->vendor, "CLeonOS");
    return (const char *)0;
}

const char *ffDetectDisks(FFDiskOptions *options, FFlist *disks) {
    FFDisk *disk;
    char mount[96];
    (void)options;
    if (cleonos_sys_disk_present() == 0ULL) {
        return "disk not present";
    }
    disk = (FFDisk *)ffListAdd(disks, sizeof(*disk));
    ffStrbufInitS(&disk->mountFrom, "disk0");
    ffStrbufInitS(&disk->name, "disk0");
    ffStrbufInitS(&disk->mountpoint, "/");
    ffStrbufInitS(&disk->filesystem, "fat32");
    ffStrbufInitS(&disk->type, "local");
    mount[0] = '\0';
    if (cleonos_sys_disk_mounted() != 0ULL && cleonos_sys_disk_mount_path(mount, (u64)sizeof(mount)) != 0ULL) {
        ffStrbufSetS(&disk->mountpoint, mount);
    }
    disk->bytesTotal = cleonos_sys_disk_size_bytes();
    disk->bytesUsed = 0;
    disk->bytesFree = disk->bytesTotal;
    disk->filesTotal = 0;
    disk->filesUsed = 0;
    disk->bytesAvailable = disk->bytesFree;
    disk->createTime = 0;
    return (const char *)0;
}

const char *ffDetectLibc(FFLibcResult *result) {
    if (result == (FFLibcResult *)0) {
        return "invalid result";
    }
    result->name = "CLeonOS libc";
    result->version = "builtin";
    return (const char *)0;
}
