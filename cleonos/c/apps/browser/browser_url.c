#include "browser_internal.h"

#include <stdio.h>
#include <string.h>

static int ush_browser_strip_fragment(const char *in_text, char *out_text, u64 out_size) {
    u64 i = 0ULL;

    if (in_text == (const char *)0 || out_text == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    while (in_text[i] != '\0' && in_text[i] != '#') {
        if (i + 1ULL >= out_size) {
            return 0;
        }
        out_text[i] = in_text[i];
        i++;
    }

    out_text[i] = '\0';
    return 1;
}

static int ush_browser_copy_until_query_or_fragment(const char *in_text, char *out_text, u64 out_size) {
    u64 i = 0ULL;

    if (in_text == (const char *)0 || out_text == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    while (in_text[i] != '\0' && in_text[i] != '?' && in_text[i] != '#') {
        if (i + 1ULL >= out_size) {
            return 0;
        }
        out_text[i] = in_text[i];
        i++;
    }

    out_text[i] = '\0';
    return 1;
}

static int ush_browser_normalize_slash_path(const char *path, char *out_path, u64 out_size) {
    char segments[USH_BROWSER_SEG_MAX][USH_BROWSER_SEG_LEN_MAX + 1U];
    u64 seg_count = 0ULL;
    u64 i = 0ULL;
    u64 out_off = 0ULL;

    if (path == (const char *)0 || out_path == (char *)0 || out_size < 2ULL) {
        return 0;
    }

    while (path[i] != '\0') {
        while (path[i] == '/') {
            i++;
        }

        if (path[i] == '\0') {
            break;
        }

        {
            u64 start = i;
            u64 len = 0ULL;

            while (path[i] != '\0' && path[i] != '/') {
                i++;
            }
            len = i - start;

            if (len == 1ULL && path[start] == '.') {
                continue;
            }

            if (len == 2ULL && path[start] == '.' && path[start + 1ULL] == '.') {
                if (seg_count > 0ULL) {
                    seg_count--;
                }
                continue;
            }

            if (len == 0ULL || len > (u64)USH_BROWSER_SEG_LEN_MAX || seg_count >= (u64)USH_BROWSER_SEG_MAX) {
                return 0;
            }

            (void)memcpy(segments[seg_count], path + start, (usize)len);
            segments[seg_count][len] = '\0';
            seg_count++;
        }
    }

    if (out_size < 2ULL) {
        return 0;
    }

    out_path[out_off++] = '/';
    if (seg_count == 0ULL) {
        out_path[out_off] = '\0';
        return 1;
    }

    for (i = 0ULL; i < seg_count; i++) {
        u64 len = ush_strlen(segments[i]);

        if (out_off + len + 1ULL >= out_size) {
            return 0;
        }

        (void)memcpy(out_path + out_off, segments[i], (usize)len);
        out_off += len;

        if (i + 1ULL < seg_count) {
            out_path[out_off++] = '/';
        }
    }

    out_path[out_off] = '\0';
    return 1;
}

static int ush_browser_join_relative_path(const char *base_path, const char *href_path, char *out_path, u64 out_size) {
    char joined[USH_BROWSER_PATH_MAX * 2U];
    char base_clean[USH_BROWSER_PATH_MAX];
    const char *slash;

    if (base_path == (const char *)0 || href_path == (const char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (href_path[0] == '/') {
        return ush_browser_normalize_slash_path(href_path, out_path, out_size);
    }

    if (ush_browser_copy_until_query_or_fragment(base_path, base_clean, (u64)sizeof(base_clean)) == 0) {
        return 0;
    }

    slash = strrchr(base_clean, '/');
    if (slash == (const char *)0) {
        if (snprintf(joined, sizeof(joined), "/%s", href_path) <= 0) {
            return 0;
        }
    } else {
        u64 dir_len = (u64)(slash - base_clean + 1);

        if (dir_len + ush_strlen(href_path) + 1ULL > (u64)sizeof(joined)) {
            return 0;
        }
        (void)memcpy(joined, base_clean, (usize)dir_len);
        joined[dir_len] = '\0';
        (void)snprintf(joined + dir_len, sizeof(joined) - (usize)dir_len, "%s", href_path);
    }

    return ush_browser_normalize_slash_path(joined, out_path, out_size);
}

static int ush_browser_resolve_http_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    ush_browser_url base;
    char href_no_frag[USH_BROWSER_LINK_HREF_MAX];
    char resolved_path[USH_BROWSER_PATH_MAX];
    const char *scheme;
    u16 default_port;

    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_strip_fragment(href, href_no_frag, (u64)sizeof(href_no_frag)) == 0) {
        return 0;
    }

    if (href_no_frag[0] == '\0' || href_no_frag[0] == '#') {
        ush_copy(out_source, out_size, base_source);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_is_http_url(href_no_frag) != 0 || ush_browser_is_https_url(href_no_frag) != 0) {
        ush_copy(out_source, out_size, href_no_frag);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_parse_url(base_source, &base) == 0) {
        return 0;
    }
    scheme = (base.tls != 0) ? "https" : "http";
    default_port = (base.tls != 0) ? 443U : 80U;

    if (href_no_frag[0] == '/' && href_no_frag[1] == '/') {
        if (snprintf(out_source, (unsigned long)out_size, "%s:%s", scheme, href_no_frag) <= 0) {
            return 0;
        }
        return 1;
    }

    if (href_no_frag[0] == '?') {
        char base_path_only[USH_BROWSER_PATH_MAX];

        if (ush_browser_copy_until_query_or_fragment(base.path, base_path_only, (u64)sizeof(base_path_only)) == 0) {
            return 0;
        }

        if (base.port == default_port) {
            if (snprintf(out_source, (unsigned long)out_size, "%s://%s%s%s", scheme, base.host, base_path_only,
                         href_no_frag) <= 0) {
                return 0;
            }
        } else {
            if (snprintf(out_source, (unsigned long)out_size, "%s://%s:%u%s%s", scheme, base.host,
                         (unsigned int)base.port, base_path_only, href_no_frag) <= 0) {
                return 0;
            }
        }
        return 1;
    }

    if (ush_browser_join_relative_path(base.path, href_no_frag, resolved_path, (u64)sizeof(resolved_path)) == 0) {
        return 0;
    }

    if (base.port == default_port) {
        if (snprintf(out_source, (unsigned long)out_size, "%s://%s%s", scheme, base.host, resolved_path) <= 0) {
            return 0;
        }
    } else {
        if (snprintf(out_source, (unsigned long)out_size, "%s://%s:%u%s", scheme, base.host, (unsigned int)base.port,
                     resolved_path) <= 0) {
            return 0;
        }
    }

    return 1;
}

static int ush_browser_resolve_local_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    char href_no_frag[USH_BROWSER_LINK_HREF_MAX];
    char base_abs[USH_PATH_MAX];
    char resolved[USH_PATH_MAX];

    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_strip_fragment(href, href_no_frag, (u64)sizeof(href_no_frag)) == 0) {
        return 0;
    }

    if (href_no_frag[0] == '\0' || href_no_frag[0] == '#') {
        ush_copy(out_source, out_size, base_source);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (href_no_frag[0] == '/') {
        if (ush_browser_normalize_slash_path(href_no_frag, resolved, (u64)sizeof(resolved)) == 0) {
            return 0;
        }
        ush_copy(out_source, out_size, resolved);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_normalize_slash_path(base_source, base_abs, (u64)sizeof(base_abs)) == 0) {
        ush_copy(base_abs, (u64)sizeof(base_abs), base_source);
    }

    if (href_no_frag[0] == '?') {
        ush_copy(out_source, out_size, base_abs);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_join_relative_path(base_abs, href_no_frag, resolved, (u64)sizeof(resolved)) == 0) {
        return 0;
    }

    ush_copy(out_source, out_size, resolved);
    return (out_source[0] != '\0') ? 1 : 0;
}

int ush_browser_resolve_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_is_http_url(href) != 0 || ush_browser_is_https_url(href) != 0) {
        ush_copy(out_source, out_size, href);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_is_http_url(base_source) != 0 || ush_browser_is_https_url(base_source) != 0) {
        return ush_browser_resolve_http_href(base_source, href, out_source, out_size);
    }

    return ush_browser_resolve_local_href(base_source, href, out_source, out_size);
}
