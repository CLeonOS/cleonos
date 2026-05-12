#include "fastfetch_cleonos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ff_port_copy(char *dst, unsigned long dst_size, const char *src) {
    if (dst == (char *)0 || dst_size == 0UL) {
        return;
    }
    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }
    (void)strncpy(dst, src, (size_t)(dst_size - 1UL));
    dst[dst_size - 1UL] = '\0';
}

int ff_port_streq(const char *left, const char *right) {
    return (left != (const char *)0 && right != (const char *)0 && strcmp(left, right) == 0) ? 1 : 0;
}

int ff_port_starts_with(const char *text, const char *prefix) {
    unsigned long i = 0UL;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

void ff_port_format_bytes(char *out, unsigned long out_size, u64 bytes) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    unsigned int unit = 0U;
    u64 whole = bytes;
    u64 frac = 0ULL;

    while (whole >= 1024ULL && unit < 4U) {
        frac = ((whole % 1024ULL) * 10ULL) / 1024ULL;
        whole /= 1024ULL;
        unit++;
    }

    if (unit == 0U) {
        (void)snprintf(out, out_size, "%llu %s", (unsigned long long)whole, units[unit]);
    } else {
        (void)snprintf(out, out_size, "%llu.%llu %s", (unsigned long long)whole, (unsigned long long)frac,
                       units[unit]);
    }
}

void ff_port_format_uptime(char *out, unsigned long out_size, u64 uptime_ms) {
    u64 total_seconds = uptime_ms / 1000ULL;
    u64 days = total_seconds / 86400ULL;
    u64 hours;
    u64 minutes;
    u64 seconds;

    total_seconds %= 86400ULL;
    hours = total_seconds / 3600ULL;
    total_seconds %= 3600ULL;
    minutes = total_seconds / 60ULL;
    seconds = total_seconds % 60ULL;

    if (days != 0ULL) {
        (void)snprintf(out, out_size, "%llu days, %llu hours, %llu mins", (unsigned long long)days,
                       (unsigned long long)hours, (unsigned long long)minutes);
    } else if (hours != 0ULL) {
        (void)snprintf(out, out_size, "%llu hours, %llu mins, %llu secs", (unsigned long long)hours,
                       (unsigned long long)minutes, (unsigned long long)seconds);
    } else if (minutes != 0ULL) {
        (void)snprintf(out, out_size, "%llu mins, %llu secs", (unsigned long long)minutes,
                       (unsigned long long)seconds);
    } else {
        (void)snprintf(out, out_size, "%llu secs", (unsigned long long)seconds);
    }
}

void ff_port_ipv4_text(char *out, unsigned long out_size, u64 ipv4_be) {
    (void)snprintf(out, out_size, "%llu.%llu.%llu.%llu", (unsigned long long)((ipv4_be >> 24U) & 0xFFULL),
                   (unsigned long long)((ipv4_be >> 16U) & 0xFFULL),
                   (unsigned long long)((ipv4_be >> 8U) & 0xFFULL), (unsigned long long)(ipv4_be & 0xFFULL));
}

int ff_port_read_file(const char *path, char *out, unsigned long out_size) {
    u64 got;

    if (out == (char *)0 || out_size == 0UL) {
        return 0;
    }
    out[0] = '\0';
    got = cleonos_sys_fs_read(path, out, (u64)(out_size - 1UL));
    if (got == 0ULL || got == (u64)-1) {
        return 0;
    }
    if (got >= (u64)out_size) {
        got = (u64)(out_size - 1UL);
    }
    out[got] = '\0';
    return 1;
}

int ff_port_env_value(const char *name, char *out, unsigned long out_size) {
    u64 envc;
    u64 i;
    unsigned long name_len;
    char entry[192];

    if (name == (const char *)0 || out == (char *)0 || out_size == 0UL) {
        return 0;
    }
    out[0] = '\0';
    name_len = (unsigned long)strlen(name);
    envc = cleonos_sys_proc_envc();
    for (i = 0ULL; i < envc; i++) {
        entry[0] = '\0';
        if (cleonos_sys_proc_env(i, entry, (u64)sizeof(entry)) == 0ULL) {
            continue;
        }
        if (strncmp(entry, name, (size_t)name_len) == 0 && entry[name_len] == '=') {
            ff_port_copy(out, out_size, entry + name_len + 1UL);
            return 1;
        }
    }
    return 0;
}

void ff_port_json_escape_print(const char *text) {
    unsigned long i = 0UL;

    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        unsigned char ch = (unsigned char)text[i++];
        if (ch == '"' || ch == '\\') {
            (void)putchar('\\');
            (void)putchar((int)ch);
        } else if (ch == '\n') {
            (void)fputs("\\n", 1);
        } else if (ch == '\r') {
            (void)fputs("\\r", 1);
        } else if (ch == '\t') {
            (void)fputs("\\t", 1);
        } else if (ch < 0x20U) {
            (void)fputs("?", 1);
        } else {
            (void)putchar((int)ch);
        }
    }
}
