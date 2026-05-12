#include "fastfetch_cleonos_compat.h"

#include <cleonos_syscall.h>

#include "common/impl/FFPlatform_private.h"

#include <string.h>

static void ff_cl_strbuf_set_cstr(FFstrbuf *buf, const char *text) {
    if (text != (const char *)0 && text[0] != '\0') {
        ffStrbufSetS(buf, text);
    }
}

void ffPlatformInitImpl(FFPlatform *platform) {
    cleonos_sysinfo info;
    cleonos_user_info user;
    char locale[CLEONOS_LOCALE_TEXT_MAX];

    (void)memset(&info, 0, sizeof(info));
    (void)memset(&user, 0, sizeof(user));
    (void)cleonos_sys_sysinfo(&info);
    (void)cleonos_sys_user_current(&user);

    platform->pid = (uint32_t)cleonos_sys_getpid();
    platform->uid = (uint32_t)user.uid;

    ff_cl_strbuf_set_cstr(&platform->homeDir, user.home[0] != '\0' ? user.home : "/");
    ffStrbufEnsureEndsWithC(&platform->homeDir, '/');
    ffStrbufSetS(&platform->cacheDir, "/temp/fastfetch/");
    ffStrbufSetS(&platform->exePath, "/shell/fastfetch.elf");
    ffStrbufSetS(&platform->cwd, "/");
    ff_cl_strbuf_set_cstr(&platform->userName, user.name[0] != '\0' ? user.name : "user");
    ff_cl_strbuf_set_cstr(&platform->fullUserName, user.name[0] != '\0' ? user.name : "CLeonOS User");
    ffStrbufSetS(&platform->hostName, "cleonos");
    ffStrbufSetS(&platform->userShell, "/shell/shell.elf");

    ffStrbufSetS(&platform->sysinfo.name, info.kernel_name[0] != '\0' ? info.kernel_name : "CLKS");
    ffStrbufSetS(&platform->sysinfo.release, info.kernel_version[0] != '\0' ? info.kernel_version : "unknown");
    if (info.build_date[0] != '\0' || info.build_time[0] != '\0') {
        ffStrbufSetF(&platform->sysinfo.version, "%s %s", info.build_date, info.build_time);
    } else {
        ffStrbufSetS(&platform->sysinfo.version, "unknown");
    }
    ffStrbufSetS(&platform->sysinfo.architecture, info.arch[0] != '\0' ? info.arch : "x86_64");
    platform->sysinfo.pageSize = 4096U;

    ffPlatformPathAddAbsolute(&platform->configDirs, "/etc/");
    ffPlatformPathAddAbsolute(&platform->dataDirs, "/system/");
    if (cleonos_sys_locale_get(locale, (u64)sizeof(locale)) != 0ULL && locale[0] != '\0') {
        (void)locale;
    }
}
