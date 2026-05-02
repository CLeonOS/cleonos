#include "browser_internal.h"

#include <stdio.h>

static int ush_browser_read_file(const ush_state *sh, const char *arg, char *out_html, u64 out_html_cap,
                                 u64 *out_size) {
    char abs_path[USH_PATH_MAX];
    u64 fd;
    u64 total = 0ULL;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0 ||
        out_html_cap == 0ULL) {
        return 0;
    }

    if (ush_resolve_path(sh, arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_stat_type(abs_path) != 1ULL) {
        return 0;
    }

    fd = cleonos_sys_fd_open(abs_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        return 0;
    }

    while (total + 1ULL < out_html_cap) {
        u64 got = cleonos_sys_fd_read(fd, out_html + total, out_html_cap - 1ULL - total);

        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        total += got;
    }

    (void)cleonos_sys_fd_close(fd);
    out_html[total] = '\0';
    *out_size = total;
    return (total > 0ULL) ? 1 : 0;
}

int ush_browser_load_source(const ush_state *sh, const char *source, char *out_html, u64 out_html_cap, u64 *out_size) {
    return ush_browser_load_request(sh, source, "GET", (const char *)0, 0ULL, out_html, out_html_cap, out_size);
}

int ush_browser_load_request(const ush_state *sh, const char *source, const char *method, const char *body,
                             u64 body_len, char *out_html, u64 out_html_cap, u64 *out_size) {
    if (sh == (const ush_state *)0 || source == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0) {
        return 0;
    }

    if (ush_browser_is_http_url(source) != 0 || ush_browser_is_https_url(source) != 0) {
        if (cleonos_sys_net_available() == 0ULL) {
            return 0;
        }
        return ush_browser_fetch_http_request(source, method, body, body_len, out_html, out_html_cap, out_size);
    }

    if (method != (const char *)0 && (method[0] == 'P' || method[0] == 'p')) {
        return 0;
    }

    return ush_browser_read_file(sh, source, out_html, out_html_cap, out_size);
}

int ush_browser_read_line(char *out_text, u64 out_size) {
    u64 len = 0ULL;

    if (out_text == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_text[0] = '\0';
    for (;;) {
        int ch = fgetc(0);

        if (ch == EOF) {
            return 0;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            (void)fputc('\n', 1);
            out_text[len] = '\0';
            return 1;
        }

        if (ch == 8 || ch == 127) {
            if (len > 0ULL) {
                len--;
                (void)fputs("\b \b", 1);
            }
            continue;
        }

        if ((unsigned int)ch < 0x20U) {
            continue;
        }

        if (len + 1ULL < out_size) {
            out_text[len++] = (char)ch;
            out_text[len] = '\0';
            (void)fputc(ch, 1);
        }
    }
}
