#include <cleonos_syscall.h>

#include <stdio.h>
#include <time.h>

static int cleonos_time_is_leap(int year_full) {
    if ((year_full % 400) == 0) {
        return 1;
    }
    if ((year_full % 100) == 0) {
        return 0;
    }
    return ((year_full % 4) == 0) ? 1 : 0;
}

static int cleonos_time_days_in_year(int year_full) {
    return (cleonos_time_is_leap(year_full) != 0) ? 366 : 365;
}

static int cleonos_time_days_in_month(int year_full, int month_zero_based) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month_zero_based == 1 && cleonos_time_is_leap(year_full) != 0) {
        return 29;
    }
    return days[month_zero_based];
}

__attribute__((weak)) time_t time(time_t *out_time) {
    time_t seconds = (time_t)(cleonos_sys_time_ms() / 1000ULL);

    if (out_time != (time_t *)0) {
        *out_time = seconds;
    }

    return seconds;
}

__attribute__((weak)) clock_t clock(void) {
    return (clock_t)cleonos_sys_time_ms();
}

__attribute__((weak)) struct tm *gmtime_r(const time_t *timer, struct tm *out_tm) {
    long long seconds;
    long long days;
    long long rem;
    int year;
    int month;
    int wday;

    if (timer == (const time_t *)0 || out_tm == (struct tm *)0) {
        return (struct tm *)0;
    }

    seconds = (long long)(*timer);
    days = seconds / 86400LL;
    rem = seconds % 86400LL;
    if (rem < 0) {
        rem += 86400LL;
        days--;
    }

    out_tm->tm_hour = (int)(rem / 3600LL);
    rem %= 3600LL;
    out_tm->tm_min = (int)(rem / 60LL);
    out_tm->tm_sec = (int)(rem % 60LL);

    wday = (int)((days + 4LL) % 7LL);
    if (wday < 0) {
        wday += 7;
    }
    out_tm->tm_wday = wday;

    year = 1970;
    while (days >= (long long)cleonos_time_days_in_year(year)) {
        days -= (long long)cleonos_time_days_in_year(year);
        year++;
    }
    while (days < 0) {
        year--;
        days += (long long)cleonos_time_days_in_year(year);
    }

    out_tm->tm_year = year - 1900;
    out_tm->tm_yday = (int)days;

    month = 0;
    while (month < 12) {
        int dim = cleonos_time_days_in_month(year, month);
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }

    out_tm->tm_mon = month;
    out_tm->tm_mday = (int)days + 1;
    out_tm->tm_isdst = 0;
    return out_tm;
}

__attribute__((weak)) struct tm *gmtime(const time_t *timer) {
    static struct tm tm_value;
    return gmtime_r(timer, &tm_value);
}

__attribute__((weak)) struct tm *localtime_r(const time_t *timer, struct tm *out_tm) {
    return gmtime_r(timer, out_tm);
}

__attribute__((weak)) struct tm *localtime(const time_t *timer) {
    static struct tm tm_value;
    return localtime_r(timer, &tm_value);
}

__attribute__((weak)) time_t mktime(struct tm *tm_value) {
    long long days = 0;
    int year;
    int month;

    if (tm_value == (struct tm *)0) {
        return (time_t)-1;
    }

    for (year = 1970; year < tm_value->tm_year + 1900; year++) {
        days += cleonos_time_days_in_year(year);
    }
    for (month = 0; month < tm_value->tm_mon; month++) {
        days += cleonos_time_days_in_month(tm_value->tm_year + 1900, month);
    }
    days += tm_value->tm_mday - 1;
    return (time_t)(days * 86400LL + (long long)tm_value->tm_hour * 3600LL + (long long)tm_value->tm_min * 60LL +
                    (long long)tm_value->tm_sec);
}

static int cleonos_time_append_num(char *out, size_t out_size, size_t *io_at, int value, int width) {
    char buf[16];
    int i;
    size_t at;
    unsigned int uvalue;

    if (out == (char *)0 || io_at == (size_t *)0) {
        return 0;
    }

    at = *io_at;
    uvalue = (value < 0) ? 0U : (unsigned int)value;
    for (i = width - 1; i >= 0; i--) {
        buf[i] = (char)('0' + (uvalue % 10U));
        uvalue /= 10U;
    }

    for (i = 0; i < width; i++) {
        if (at + 1U >= out_size) {
            return 0;
        }
        out[at++] = buf[i];
    }

    *io_at = at;
    return 1;
}

__attribute__((weak)) size_t strftime(char *out, size_t out_size, const char *format, const struct tm *tm_value) {
    size_t at = 0U;
    size_t i = 0U;

    if (out == (char *)0 || out_size == 0U || format == (const char *)0 || tm_value == (const struct tm *)0) {
        return 0U;
    }

    while (format[i] != '\0') {
        if (format[i] != '%') {
            if (at + 1U >= out_size) {
                out[0] = '\0';
                return 0U;
            }
            out[at++] = format[i++];
            continue;
        }

        i++;
        switch (format[i]) {
        case 'Y':
            if (cleonos_time_append_num(out, out_size, &at, tm_value->tm_year + 1900, 4) == 0) {
                out[0] = '\0';
                return 0U;
            }
            break;
        case 'm':
            if (cleonos_time_append_num(out, out_size, &at, tm_value->tm_mon + 1, 2) == 0) {
                out[0] = '\0';
                return 0U;
            }
            break;
        case 'd':
            if (cleonos_time_append_num(out, out_size, &at, tm_value->tm_mday, 2) == 0) {
                out[0] = '\0';
                return 0U;
            }
            break;
        case 'H':
            if (cleonos_time_append_num(out, out_size, &at, tm_value->tm_hour, 2) == 0) {
                out[0] = '\0';
                return 0U;
            }
            break;
        case 'M':
            if (cleonos_time_append_num(out, out_size, &at, tm_value->tm_min, 2) == 0) {
                out[0] = '\0';
                return 0U;
            }
            break;
        case 'S':
            if (cleonos_time_append_num(out, out_size, &at, tm_value->tm_sec, 2) == 0) {
                out[0] = '\0';
                return 0U;
            }
            break;
        case '%':
            if (at + 1U >= out_size) {
                out[0] = '\0';
                return 0U;
            }
            out[at++] = '%';
            break;
        default:
            out[0] = '\0';
            return 0U;
        }
        i++;
    }

    out[at] = '\0';
    return at;
}
