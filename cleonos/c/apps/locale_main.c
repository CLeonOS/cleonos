#include "cmd_runtime.h"

static int locale_valid_local(const char *locale) {
    u64 i;
    u64 len;

    if (locale == (const char *)0 || locale[0] == '\0') {
        return 0;
    }

    len = ush_strlen(locale);
    if (len >= CLEONOS_LOCALE_TEXT_MAX) {
        return 0;
    }

    for (i = 0ULL; i < len; i++) {
        char ch = locale[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-' || ch == '.') {
            continue;
        }
        return 0;
    }

    return 1;
}

static int locale_show(void) {
    char locale[CLEONOS_LOCALE_TEXT_MAX];
    u64 got;

    ush_zero(locale, (u64)sizeof(locale));
    got = cleonos_sys_locale_get(locale, (u64)sizeof(locale));
    if (got == 0ULL) {
        ush_writeln_i18n("locale: syscall failed", "locale: 系统调用失败");
        return 0;
    }

    ush_write_i18n_label("LANG", "系统语言");
    ush_write("=");
    ush_writeln(locale);
    return 1;
}

static int locale_set(const char *value) {
    u64 ret;

    if (locale_valid_local(value) == 0) {
        ush_writeln_i18n("locale: invalid locale", "locale: 无效的 locale");
        return 0;
    }

    ret = cleonos_sys_locale_set(value);
    if (ret == 1ULL) {
        ush_write_i18n_label("locale: set to", "locale: 已设置为");
        ush_write(" ");
        ush_writeln(value);
        return 1;
    }

    if (ret == 2ULL) {
        ush_write_i18n_label("locale: set in kernel, but failed to persist /system/locale.conf",
                             "locale: 内核已设置，但写入 /system/locale.conf 失败");
        ush_write(": ");
        ush_writeln(value);
        return 1;
    }

    ush_writeln_i18n("locale: set failed", "locale: 设置失败");
    return 0;
}

static int locale_usage(void) {
    ush_writeln_i18n("usage:", "用法:");
    ush_writeln("  locale");
    ush_writeln("  locale get");
    ush_writeln("  locale set <locale>");
    ush_writeln_i18n("examples:", "示例:");
    ush_writeln("  locale set zh_CN.UTF-8");
    ush_writeln("  locale set en_US.UTF-8");
    return 0;
}

static int ush_cmd_locale(const char *arg) {
    char first[32];
    const char *rest;

    if (arg == (const char *)0 || arg[0] == '\0') {
        return locale_show();
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return locale_usage();
    }

    if (ush_streq(first, "get") != 0) {
        if (rest[0] != '\0') {
            return locale_usage();
        }
        return locale_show();
    }

    if (ush_streq(first, "set") != 0) {
        char value[CLEONOS_LOCALE_TEXT_MAX];
        const char *extra;

        if (ush_split_first_and_rest(rest, value, (u64)sizeof(value), &extra) == 0 || extra[0] != '\0') {
            return locale_usage();
        }

        return locale_set(value);
    }

    return locale_usage();
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    const char *arg = "";
    int success;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cwd[0] == '/') {
            ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
        }
        arg = ctx.arg;
    }

    success = ush_cmd_locale(arg);
    ret.exit_code = (success != 0) ? 0ULL : 1ULL;
    (void)ush_command_ret_write(&ret);
    return success != 0 ? 0 : 1;
}
