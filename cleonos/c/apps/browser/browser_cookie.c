#include "browser_internal.h"

#include <stdio.h>
#include <string.h>

static int ush_browser_cookie_streq_icase(const char *left, const char *right) {
    u64 i = 0ULL;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (ush_browser_ascii_tolower(left[i]) != ush_browser_ascii_tolower(right[i])) {
            return 0;
        }
        i++;
    }

    return (left[i] == '\0' && right[i] == '\0') ? 1 : 0;
}

static int ush_browser_cookie_starts_with_icase(const char *text, u64 text_len, const char *prefix) {
    u64 i = 0ULL;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (i >= text_len || ush_browser_ascii_tolower(text[i]) != ush_browser_ascii_tolower(prefix[i])) {
            return 0;
        }
        i++;
    }

    return 1;
}

static void ush_browser_cookie_copy_trimmed(char *out, u64 out_size, const char *text, u64 len) {
    u64 start = 0ULL;
    u64 end = len;
    u64 copy_len;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    out[0] = '\0';
    if (text == (const char *)0) {
        return;
    }

    while (start < end && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n')) {
        start++;
    }
    while (end > start &&
           (text[end - 1ULL] == ' ' || text[end - 1ULL] == '\t' || text[end - 1ULL] == '\r' ||
            text[end - 1ULL] == '\n')) {
        end--;
    }

    copy_len = end - start;
    if (copy_len + 1ULL > out_size) {
        copy_len = out_size - 1ULL;
    }
    if (copy_len > 0ULL) {
        (void)memcpy(out, text + start, (usize)copy_len);
    }
    out[copy_len] = '\0';
}

static int ush_browser_cookie_domain_matches(const char *host, const char *domain) {
    u64 host_len;
    u64 domain_len;

    if (host == (const char *)0 || domain == (const char *)0 || host[0] == '\0' || domain[0] == '\0') {
        return 0;
    }

    if (ush_browser_cookie_streq_icase(host, domain) != 0) {
        return 1;
    }

    host_len = ush_strlen(host);
    domain_len = ush_strlen(domain);
    if (domain[0] == '.') {
        domain++;
        domain_len--;
    }
    if (domain_len == 0ULL || host_len <= domain_len) {
        return 0;
    }
    if (host[host_len - domain_len - 1ULL] != '.') {
        return 0;
    }

    return (ush_browser_cookie_streq_icase(host + host_len - domain_len, domain) != 0) ? 1 : 0;
}

static int ush_browser_cookie_path_matches(const char *request_path, const char *cookie_path) {
    u64 i;

    if (request_path == (const char *)0 || cookie_path == (const char *)0 || cookie_path[0] == '\0') {
        return 0;
    }

    for (i = 0ULL; cookie_path[i] != '\0'; i++) {
        if (request_path[i] != cookie_path[i]) {
            return 0;
        }
    }
    return 1;
}

static void ush_browser_cookie_default_path(const char *request_path, char *out, u64 out_size) {
    u64 len;
    u64 i;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    out[0] = '/';
    out[1] = '\0';
    if (request_path == (const char *)0 || request_path[0] != '/') {
        return;
    }

    len = ush_strlen(request_path);
    for (i = len; i > 0ULL; i--) {
        if (request_path[i - 1ULL] == '/') {
            u64 copy_len = (i == 1ULL) ? 1ULL : i - 1ULL;
            if (copy_len + 1ULL > out_size) {
                copy_len = out_size - 1ULL;
            }
            (void)memcpy(out, request_path, (usize)copy_len);
            out[copy_len] = '\0';
            return;
        }
    }
}

static ush_browser_cookie *ush_browser_cookie_find_slot(const char *name, const char *domain, const char *path) {
    u64 i;

    for (i = 0ULL; i < ush_browser_cookie_count; i++) {
        ush_browser_cookie *cookie = &ush_browser_cookies[i];
        if (ush_browser_cookie_streq_icase(cookie->name, name) != 0 &&
            ush_browser_cookie_streq_icase(cookie->domain, domain) != 0 && ush_streq(cookie->path, path) != 0) {
            return cookie;
        }
    }

    if (ush_browser_cookie_count < (u64)USH_BROWSER_COOKIE_MAX) {
        return &ush_browser_cookies[ush_browser_cookie_count++];
    }

    return &ush_browser_cookies[(u64)USH_BROWSER_COOKIE_MAX - 1ULL];
}

static void ush_browser_cookie_store_one(const ush_browser_url *url, const char *value, u64 value_len) {
    char pair[USH_BROWSER_COOKIE_NAME_MAX + USH_BROWSER_COOKIE_VALUE_MAX + 8U];
    char name[USH_BROWSER_COOKIE_NAME_MAX];
    char cookie_value[USH_BROWSER_COOKIE_VALUE_MAX];
    char domain[USH_BROWSER_COOKIE_DOMAIN_MAX];
    char path[USH_BROWSER_COOKIE_PATH_MAX];
    const char *cursor;
    u64 pair_len = 0ULL;
    u64 eq_pos = (u64)-1;
    int secure = 0;
    ush_browser_cookie *slot;

    if (url == (const ush_browser_url *)0 || value == (const char *)0 || value_len == 0ULL) {
        return;
    }

    while (pair_len < value_len && value[pair_len] != ';') {
        pair_len++;
    }
    ush_browser_cookie_copy_trimmed(pair, (u64)sizeof(pair), value, pair_len);
    for (eq_pos = 0ULL; pair[eq_pos] != '\0'; eq_pos++) {
        if (pair[eq_pos] == '=') {
            break;
        }
    }
    if (pair[eq_pos] != '=' || eq_pos == 0ULL) {
        return;
    }

    ush_browser_cookie_copy_trimmed(name, (u64)sizeof(name), pair, eq_pos);
    ush_browser_cookie_copy_trimmed(cookie_value, (u64)sizeof(cookie_value), pair + eq_pos + 1ULL,
                                    ush_strlen(pair + eq_pos + 1ULL));
    ush_copy(domain, (u64)sizeof(domain), url->host);
    ush_browser_cookie_default_path(url->path, path, (u64)sizeof(path));

    cursor = value + pair_len;
    while ((u64)(cursor - value) < value_len) {
        const char *attr_start;
        u64 attr_len = 0ULL;
        char attr[128];

        if (*cursor == ';') {
            cursor++;
        }
        while ((u64)(cursor - value) < value_len && (*cursor == ' ' || *cursor == '\t')) {
            cursor++;
        }
        attr_start = cursor;
        while ((u64)(cursor - value) < value_len && *cursor != ';') {
            cursor++;
            attr_len++;
        }
        ush_browser_cookie_copy_trimmed(attr, (u64)sizeof(attr), attr_start, attr_len);
        if (attr[0] == '\0') {
            continue;
        }

        if (ush_browser_cookie_streq_icase(attr, "secure") != 0) {
            secure = 1;
        } else if (ush_browser_cookie_starts_with_icase(attr, ush_strlen(attr), "domain=") != 0) {
            char *eq = strchr(attr, '=');
            if (eq != (char *)0 && eq[1] != '\0') {
                ush_browser_cookie_copy_trimmed(domain, (u64)sizeof(domain), eq + 1, ush_strlen(eq + 1));
            }
        } else if (ush_browser_cookie_starts_with_icase(attr, ush_strlen(attr), "path=") != 0) {
            char *eq = strchr(attr, '=');
            if (eq != (char *)0 && eq[1] != '\0') {
                ush_browser_cookie_copy_trimmed(path, (u64)sizeof(path), eq + 1, ush_strlen(eq + 1));
            }
        }
    }

    if (name[0] == '\0' || domain[0] == '\0' || path[0] == '\0') {
        return;
    }

    slot = ush_browser_cookie_find_slot(name, domain, path);
    if (slot == (ush_browser_cookie *)0) {
        return;
    }
    ush_zero(slot, (u64)sizeof(*slot));
    ush_copy(slot->name, (u64)sizeof(slot->name), name);
    ush_copy(slot->value, (u64)sizeof(slot->value), cookie_value);
    ush_copy(slot->domain, (u64)sizeof(slot->domain), domain);
    ush_copy(slot->path, (u64)sizeof(slot->path), path);
    slot->secure = secure;
}

void ush_browser_cookie_clear_all(void) {
    ush_zero(ush_browser_cookies, (u64)sizeof(ush_browser_cookies));
    ush_browser_cookie_count = 0ULL;
}

void ush_browser_cookie_set_token(const char *token) {
    ush_copy(ush_browser_bearer_token, (u64)sizeof(ush_browser_bearer_token), (token != (const char *)0) ? token : "");
}

void ush_browser_cookie_clear_token(void) {
    ush_browser_bearer_token[0] = '\0';
}

int ush_browser_cookie_build_header(const ush_browser_url *url, char *out_header, u64 out_size) {
    u64 i;
    u64 used = 0ULL;
    int first = 1;

    if (url == (const ush_browser_url *)0 || out_header == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_header[0] = '\0';
    for (i = 0ULL; i < ush_browser_cookie_count; i++) {
        ush_browser_cookie *cookie = &ush_browser_cookies[i];
        u64 need;

        if (cookie->name[0] == '\0' || cookie->value[0] == '\0') {
            continue;
        }
        if (cookie->secure != 0 && url->tls == 0) {
            continue;
        }
        if (ush_browser_cookie_domain_matches(url->host, cookie->domain) == 0 ||
            ush_browser_cookie_path_matches(url->path, cookie->path) == 0) {
            continue;
        }

        need = ush_strlen(cookie->name) + ush_strlen(cookie->value) + (first != 0 ? 2ULL : 4ULL);
        if (used + need + 1ULL > out_size) {
            break;
        }
        if (first != 0) {
            (void)snprintf(out_header + used, (unsigned long)(out_size - used), "%s=%s", cookie->name, cookie->value);
        } else {
            (void)snprintf(out_header + used, (unsigned long)(out_size - used), "; %s=%s", cookie->name,
                           cookie->value);
        }
        used = ush_strlen(out_header);
        first = 0;
    }

    return (out_header[0] != '\0') ? 1 : 0;
}

void ush_browser_cookie_store_from_response(const ush_browser_url *url, const char *raw, u64 raw_len) {
    u64 header_end = 0ULL;
    u64 off = 0ULL;
    u64 i;

    if (url == (const ush_browser_url *)0 || raw == (const char *)0) {
        return;
    }

    for (i = 0ULL; i + 3ULL < raw_len; i++) {
        if (raw[i] == '\r' && raw[i + 1ULL] == '\n' && raw[i + 2ULL] == '\r' && raw[i + 3ULL] == '\n') {
            header_end = i + 4ULL;
            break;
        }
    }
    if (header_end == 0ULL) {
        return;
    }

    while (off + 1ULL < header_end) {
        u64 line_start = off;
        u64 line_len;

        while (off + 1ULL < header_end && !(raw[off] == '\r' && raw[off + 1ULL] == '\n')) {
            off++;
        }
        if (off + 1ULL >= header_end) {
            break;
        }
        line_len = off - line_start;
        off += 2ULL;

        if (line_len > 11ULL && ush_browser_cookie_starts_with_icase(raw + line_start, line_len, "Set-Cookie:") != 0) {
            const char *value = raw + line_start + 11ULL;
            u64 value_len = line_len - 11ULL;
            ush_browser_cookie_store_one(url, value, value_len);
        }
    }
}

void ush_browser_cookie_print_state(void) {
    u64 i;

    (void)printf("[browser] cookies: %llu\n", (unsigned long long)ush_browser_cookie_count);
    for (i = 0ULL; i < ush_browser_cookie_count; i++) {
        ush_browser_cookie *cookie = &ush_browser_cookies[i];
        (void)printf("  %s=%s domain=%s path=%s%s\n", cookie->name, cookie->value, cookie->domain, cookie->path,
                     (cookie->secure != 0) ? " secure" : "");
    }
    (void)printf("[browser] bearer token: %s\n", (ush_browser_bearer_token[0] != '\0') ? "set" : "not set");
}
