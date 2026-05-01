#include "browser_internal.h"

int ush_browser_is_http_url(const char *text) {
    if (text == (const char *)0) {
        return 0;
    }

    return (text[0] == 'h' && text[1] == 't' && text[2] == 't' && text[3] == 'p' && text[4] == ':' && text[5] == '/' &&
            text[6] == '/')
               ? 1
               : 0;
}

int ush_browser_is_https_url(const char *text) {
    if (text == (const char *)0) {
        return 0;
    }

    return (text[0] == 'h' && text[1] == 't' && text[2] == 't' && text[3] == 'p' && text[4] == 's' && text[5] == ':' &&
            text[6] == '/' && text[7] == '/')
               ? 1
               : 0;
}

u16 ush_browser_read_be16(const u8 *ptr) {
    return (u16)(((u16)ptr[0] << 8U) | (u16)ptr[1]);
}

void ush_browser_write_be16(u8 *ptr, u16 value) {
    ptr[0] = (u8)((value >> 8U) & 0xFFU);
    ptr[1] = (u8)(value & 0xFFU);
}

int ush_browser_parse_ipv4_be(const char *text, u64 *out_ipv4_be) {
    u64 acc = 0ULL;
    u64 value = 0ULL;
    u64 parts = 0ULL;
    u64 has_digit = 0ULL;
    u64 i = 0ULL;

    if (text == (const char *)0 || out_ipv4_be == (u64 *)0) {
        return 0;
    }

    while (text[i] != '\0') {
        char ch = text[i];

        if (ch >= '0' && ch <= '9') {
            value = (value * 10ULL) + (u64)(ch - '0');
            if (value > 255ULL) {
                return 0;
            }
            has_digit = 1ULL;
        } else if (ch == '.') {
            if (has_digit == 0ULL || parts >= 3ULL) {
                return 0;
            }
            acc = (acc << 8ULL) | value;
            parts++;
            value = 0ULL;
            has_digit = 0ULL;
        } else {
            return 0;
        }

        i++;
    }

    if (has_digit == 0ULL || parts != 3ULL) {
        return 0;
    }

    acc = (acc << 8ULL) | value;
    *out_ipv4_be = acc & 0xFFFFFFFFULL;
    return 1;
}

int ush_browser_parse_url(const char *url, ush_browser_url *out_url) {
    const char *p;
    const char *host_begin;
    const char *host_end;
    const char *path_begin;
    u64 host_len;
    u64 path_len;
    u64 i;
    u64 scheme_len;

    if (url == (const char *)0 || out_url == (ush_browser_url *)0) {
        return 0;
    }

    ush_zero(out_url, (u64)sizeof(*out_url));

    if (url[0] == '\0') {
        return 0;
    }

    if (ush_browser_is_http_url(url) != 0) {
        out_url->tls = 0;
        out_url->port = 80U;
        scheme_len = 7ULL;
    } else if (ush_browser_is_https_url(url) != 0) {
        out_url->tls = 1;
        out_url->port = 443U;
        scheme_len = 8ULL;
    } else {
        return 0;
    }

    p = url + scheme_len;
    host_begin = p;

    while (*p != '\0' && *p != '/') {
        p++;
    }

    host_end = p;
    host_len = (u64)(host_end - host_begin);
    if (host_len == 0ULL || host_len >= (u64)sizeof(out_url->host)) {
        return 0;
    }

    for (i = 0ULL; i < host_len; i++) {
        out_url->host[i] = host_begin[i];
    }
    out_url->host[host_len] = '\0';

    for (i = 0ULL; i < host_len; i++) {
        if (out_url->host[i] == ':') {
            u64 parsed_port = 0ULL;

            if (i == 0ULL || i + 1ULL >= host_len) {
                return 0;
            }

            out_url->host[i] = '\0';
            if (ush_parse_u64_dec(out_url->host + i + 1ULL, &parsed_port) == 0 || parsed_port == 0ULL ||
                parsed_port > 65535ULL) {
                return 0;
            }

            out_url->port = (u16)parsed_port;
            break;
        }
    }

    if (out_url->host[0] == '\0') {
        return 0;
    }

    path_begin = (*p == '/') ? p : "/";
    path_len = ush_strlen(path_begin);
    if (path_len == 0ULL || path_len >= (u64)sizeof(out_url->path)) {
        return 0;
    }

    for (i = 0ULL; i < path_len; i++) {
        out_url->path[i] = path_begin[i];
    }
    out_url->path[path_len] = '\0';

    return 1;
}
