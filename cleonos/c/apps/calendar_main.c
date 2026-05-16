#include "user_cmd_common.h"
#include <time.h>

static int cal_is_leap_year(int year) {
    if ((year % 400) == 0) {
        return 1;
    }
    if ((year % 100) == 0) {
        return 0;
    }
    return ((year % 4) == 0) ? 1 : 0;
}

static int cal_days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && cal_is_leap_year(year) != 0) {
        return 29;
    }

    return days[month - 1];
}

static int cal_weekday(int year, int month, int day) {
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    if (month < 3) {
        year--;
    }

    return (year + (year / 4) - (year / 100) + (year / 400) + offsets[month - 1] + day) % 7;
}

static const char *cal_month_name_en(int month) {
    static const char *names[] = {"",     "January",   "February", "March",    "April",   "May",
                                  "June",  "July",      "August",   "September", "October", "November",
                                  "December"};

    if (month < 1 || month > 12) {
        return "";
    }

    return names[month];
}

static int cal_parse_month_year(const char *arg, int *out_month, int *out_year) {
    char first[32];
    const char *rest = "";
    u64 month = 0ULL;
    u64 year = 0ULL;

    if (out_month == (int *)0 || out_year == (int *)0) {
        return 0;
    }

    if (arg == (const char *)0 || arg[0] == '\0') {
        time_t now = time((time_t *)0);
        struct tm *tmv = localtime(&now);
        if (tmv == (struct tm *)0) {
            return 0;
        }
        *out_month = tmv->tm_mon + 1;
        *out_year = tmv->tm_year + 1900;
        return 1;
    }

    first[0] = '\0';
    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (first[0] == '\0') {
        return 0;
    }

    if (ush_parse_u64_dec(first, &month) == 0) {
        return 0;
    }

    if (rest == (const char *)0 || rest[0] == '\0') {
        time_t now = time((time_t *)0);
        struct tm *tmv = localtime(&now);
        if (tmv == (struct tm *)0) {
            return 0;
        }
        year = (u64)(tmv->tm_year + 1900);
    } else if (ush_parse_u64_dec(rest, &year) == 0) {
        return 0;
    }

    if (month < 1ULL || month > 12ULL || year < 1900ULL || year > 9999ULL) {
        return 0;
    }

    *out_month = (int)month;
    *out_year = (int)year;
    return 1;
}

static void cal_print_header(int month, int year) {
    char text[64];

    if (ush_locale_is_zh() != 0) {
        (void)snprintf(text, sizeof(text), "%d年%d月", year, month);
    } else {
        (void)snprintf(text, sizeof(text), "%s %d", cal_month_name_en(month), year);
    }

    ush_writeln(text);
}

static void cal_print_weekdays(void) {
    if (ush_locale_is_zh() != 0) {
        ush_writeln("日 一 二 三 四 五 六");
    } else {
        ush_writeln("Su Mo Tu We Th Fr Sa");
    }
}

static void cal_print_month(int month, int year) {
    int days = cal_days_in_month(year, month);
    int start_wday = cal_weekday(year, month, 1);
    int today_month = -1;
    int today_year = -1;
    int today_day = -1;
    int is_current_month = 0;
    int day;
    time_t now = time((time_t *)0);
    struct tm *tmv = localtime(&now);

    if (tmv != (struct tm *)0) {
        today_month = tmv->tm_mon + 1;
        today_year = tmv->tm_year + 1900;
        today_day = tmv->tm_mday;
        is_current_month = (today_month == month && today_year == year) ? 1 : 0;
    }

    cal_print_header(month, year);
    cal_print_weekdays();

    for (day = 0; day < start_wday; day++) {
        ush_write("   ");
    }

    for (day = 1; day <= days; day++) {
        char cell[16];
        int highlight = (is_current_month != 0 && day == today_day) ? 1 : 0;

        if (highlight != 0) {
            ush_write("\x1B[7m");
        }

        (void)snprintf(cell, sizeof(cell), "%2d", day);
        ush_write(cell);

        if (highlight != 0) {
            ush_write("\x1B[0m");
        }

        if (((start_wday + day - 1) % 7) == 6) {
            ush_write_char('\n');
        } else {
            ush_write(" ");
        }
    }

    if (((start_wday + days - 1) % 7) != 6) {
        ush_write_char('\n');
    }
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int month = 0;
    int year = 0;
    int success;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "calendar") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = cal_parse_month_year(arg, &month, &year);
    if (success != 0) {
        cal_print_month(month, year);
    } else {
        ush_writeln_i18n("calendar: usage calendar [month] [year]", "calendar: 用法 calendar [月份] [年份]");
    }

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
