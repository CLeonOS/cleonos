#include "browser_internal.h"

#include <string.h>

static int ush_browser_header_name_match_icase(const char *line, u64 line_len, const char *name) {
    u64 i = 0ULL;

    if (line == (const char *)0 || name == (const char *)0) {
        return 0;
    }

    while (name[i] != '\0') {
        if (i >= line_len) {
            return 0;
        }
        if (ush_browser_ascii_tolower(line[i]) != ush_browser_ascii_tolower(name[i])) {
            return 0;
        }
        i++;
    }

    if (i >= line_len || line[i] != ':') {
        return 0;
    }

    return 1;
}

static int ush_browser_parse_content_length(const char *value, u64 value_len, u64 *out_len) {
    u64 i = 0ULL;
    u64 acc = 0ULL;
    int has_digit = 0;

    if (value == (const char *)0 || out_len == (u64 *)0) {
        return 0;
    }

    while (i < value_len && (value[i] == ' ' || value[i] == '\t')) {
        i++;
    }

    while (i < value_len && value[i] >= '0' && value[i] <= '9') {
        u64 next = acc * 10ULL + (u64)(value[i] - '0');
        if (next < acc) {
            return 0;
        }
        acc = next;
        has_digit = 1;
        i++;
    }

    if (has_digit == 0) {
        return 0;
    }

    *out_len = acc;
    return 1;
}

int ush_browser_http_status_code(const char *raw, u64 raw_len) {
    u64 i = 0ULL;

    if (raw == (const char *)0 || raw_len < 12ULL) {
        return 0;
    }
    if (!(raw[0] == 'H' && raw[1] == 'T' && raw[2] == 'T' && raw[3] == 'P')) {
        return 0;
    }

    while (i < raw_len && raw[i] != ' ' && raw[i] != '\r' && raw[i] != '\n') {
        i++;
    }
    while (i < raw_len && raw[i] == ' ') {
        i++;
    }
    if (i + 2ULL >= raw_len || raw[i] < '0' || raw[i] > '9' || raw[i + 1ULL] < '0' || raw[i + 1ULL] > '9' ||
        raw[i + 2ULL] < '0' || raw[i + 2ULL] > '9') {
        return 0;
    }

    return ((int)(raw[i] - '0') * 100) + ((int)(raw[i + 1ULL] - '0') * 10) + (int)(raw[i + 2ULL] - '0');
}

int ush_browser_copy_http_header_value(const char *raw, u64 raw_len, const char *name, char *out, u64 out_cap) {
    u64 header_end = 0ULL;
    u64 i;
    u64 off;

    if (raw == (const char *)0 || name == (const char *)0 || out == (char *)0 || out_cap == 0ULL) {
        return 0;
    }
    out[0] = '\0';

    for (i = 0ULL; i + 3ULL < raw_len; i++) {
        if (raw[i] == '\r' && raw[i + 1ULL] == '\n' && raw[i + 2ULL] == '\r' && raw[i + 3ULL] == '\n') {
            header_end = i + 4ULL;
            break;
        }
    }
    if (header_end == 0ULL) {
        return 0;
    }

    off = 0ULL;
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
        if (line_len == 0ULL) {
            continue;
        }

        if (ush_browser_header_name_match_icase(raw + line_start, line_len, name) != 0) {
            u64 key_len = ush_strlen(name) + 1ULL;
            const char *value = raw + line_start + key_len;
            u64 value_len = line_len - key_len;
            u64 copy_len;

            while (value_len > 0ULL && (*value == ' ' || *value == '\t')) {
                value++;
                value_len--;
            }
            while (value_len > 0ULL && (value[value_len - 1ULL] == ' ' || value[value_len - 1ULL] == '\t')) {
                value_len--;
            }

            copy_len = value_len;
            if (copy_len + 1ULL > out_cap) {
                copy_len = out_cap - 1ULL;
            }
            (void)memcpy(out, value, (usize)copy_len);
            out[copy_len] = '\0';
            return (copy_len > 0ULL) ? 1 : 0;
        }
    }

    return 0;
}

int ush_browser_parse_http_headers(const char *raw, u64 raw_len, u64 *out_body_off, int *out_chunked,
                                   u64 *out_content_len, int *out_has_content_len, int *out_compressed) {
    u64 header_end = 0ULL;
    u64 i;
    u64 off;

    if (raw == (const char *)0 || out_body_off == (u64 *)0 || out_chunked == (int *)0 || out_content_len == (u64 *)0 ||
        out_has_content_len == (int *)0 || out_compressed == (int *)0) {
        return 0;
    }

    *out_body_off = 0ULL;
    *out_chunked = 0;
    *out_content_len = 0ULL;
    *out_has_content_len = 0;
    *out_compressed = 0;

    for (i = 0ULL; i + 3ULL < raw_len; i++) {
        if (raw[i] == '\r' && raw[i + 1ULL] == '\n' && raw[i + 2ULL] == '\r' && raw[i + 3ULL] == '\n') {
            header_end = i + 4ULL;
            break;
        }
    }

    if (header_end == 0ULL || header_end > raw_len) {
        return 0;
    }

    off = 0ULL;
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

        if (line_len == 0ULL) {
            continue;
        }

        if (ush_browser_header_name_match_icase(raw + line_start, line_len, "Transfer-Encoding") != 0) {
            u64 key_len = (u64)(sizeof("Transfer-Encoding") - 1U + 1U);
            const char *value = raw + line_start + key_len;
            u64 value_len = line_len - key_len;

            while (value_len > 0ULL && (*value == ' ' || *value == '\t')) {
                value++;
                value_len--;
            }

            if (ush_browser_line_has_token_icase(value, value_len, "chunked") != 0) {
                *out_chunked = 1;
            }
        } else if (ush_browser_header_name_match_icase(raw + line_start, line_len, "Content-Encoding") != 0) {
            u64 key_len = (u64)(sizeof("Content-Encoding") - 1U + 1U);
            const char *value = raw + line_start + key_len;
            u64 value_len = line_len - key_len;

            while (value_len > 0ULL && (*value == ' ' || *value == '\t')) {
                value++;
                value_len--;
            }

            if (ush_browser_line_has_token_icase(value, value_len, "gzip") != 0 ||
                ush_browser_line_has_token_icase(value, value_len, "deflate") != 0 ||
                ush_browser_line_has_token_icase(value, value_len, "br") != 0) {
                *out_compressed = 1;
            }
        } else if (ush_browser_header_name_match_icase(raw + line_start, line_len, "Content-Length") != 0) {
            u64 key_len = (u64)(sizeof("Content-Length") - 1U + 1U);
            const char *value = raw + line_start + key_len;
            u64 value_len = line_len - key_len;
            u64 parsed = 0ULL;

            while (value_len > 0ULL && (*value == ' ' || *value == '\t')) {
                value++;
                value_len--;
            }

            if (ush_browser_parse_content_length(value, value_len, &parsed) != 0) {
                *out_content_len = parsed;
                *out_has_content_len = 1;
            }
        }
    }

    *out_body_off = header_end;
    return 1;
}

int ush_browser_find_http_header_end(const char *raw, u64 raw_len, u64 *out_body_off) {
    u64 i;

    if (raw == (const char *)0 || out_body_off == (u64 *)0) {
        return 0;
    }

    for (i = 0ULL; i + 3ULL < raw_len; i++) {
        if (raw[i] == '\r' && raw[i + 1ULL] == '\n' && raw[i + 2ULL] == '\r' && raw[i + 3ULL] == '\n') {
            *out_body_off = i + 4ULL;
            return 1;
        }
    }

    return 0;
}

/* return: 1 complete, 0 incomplete, -1 invalid */
int ush_browser_is_chunked_body_complete(const char *body, u64 body_len) {
    u64 in_off = 0ULL;

    if (body == (const char *)0) {
        return -1;
    }

    while (in_off < body_len) {
        u64 line_end = in_off;
        u64 chunk_size = 0ULL;
        int has_digit = 0;
        u64 i;

        while (line_end + 1ULL < body_len && !(body[line_end] == '\r' && body[line_end + 1ULL] == '\n')) {
            line_end++;
        }
        if (line_end + 1ULL >= body_len) {
            return 0;
        }

        for (i = in_off; i < line_end; i++) {
            char ch = body[i];
            u64 v = 0ULL;

            if (ch == ';') {
                break;
            }
            if (ch == ' ' || ch == '\t') {
                if (has_digit != 0) {
                    break;
                }
                continue;
            }

            if (ch >= '0' && ch <= '9') {
                v = (u64)(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                v = (u64)(ch - 'a') + 10ULL;
            } else if (ch >= 'A' && ch <= 'F') {
                v = (u64)(ch - 'A') + 10ULL;
            } else {
                return -1;
            }

            if (chunk_size > (0xFFFFFFFFFFFFFFFFULL >> 4U)) {
                return -1;
            }
            chunk_size = (chunk_size << 4U) | v;
            has_digit = 1;
        }

        if (has_digit == 0) {
            return -1;
        }

        in_off = line_end + 2ULL;
        if (chunk_size == 0ULL) {
            for (;;) {
                line_end = in_off;
                while (line_end + 1ULL < body_len && !(body[line_end] == '\r' && body[line_end + 1ULL] == '\n')) {
                    line_end++;
                }
                if (line_end + 1ULL >= body_len) {
                    return 0;
                }

                if (line_end == in_off) {
                    return 1;
                }

                in_off = line_end + 2ULL;
            }
        }

        if (in_off + chunk_size + 2ULL > body_len) {
            return 0;
        }
        if (body[in_off + chunk_size] != '\r' || body[in_off + chunk_size + 1ULL] != '\n') {
            return -1;
        }
        in_off += chunk_size + 2ULL;
    }

    return 0;
}

int ush_browser_decode_chunked_body(const char *body, u64 body_len, char *out, u64 out_cap, u64 *out_len) {
    u64 in_off = 0ULL;
    u64 out_off = 0ULL;

    if (body == (const char *)0 || out == (char *)0 || out_len == (u64 *)0 || out_cap == 0ULL) {
        return 0;
    }

    while (in_off < body_len) {
        u64 line_end = in_off;
        u64 chunk_size = 0ULL;
        int has_digit = 0;
        u64 i;

        while (line_end + 1ULL < body_len && !(body[line_end] == '\r' && body[line_end + 1ULL] == '\n')) {
            line_end++;
        }
        if (line_end + 1ULL >= body_len) {
            break;
        }

        for (i = in_off; i < line_end; i++) {
            char ch = body[i];
            u64 v = 0ULL;

            if (ch == ';') {
                break;
            }
            if (ch == ' ' || ch == '\t') {
                if (has_digit != 0) {
                    break;
                }
                continue;
            }

            if (ch >= '0' && ch <= '9') {
                v = (u64)(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                v = (u64)(ch - 'a') + 10ULL;
            } else if (ch >= 'A' && ch <= 'F') {
                v = (u64)(ch - 'A') + 10ULL;
            } else {
                break;
            }

            if (chunk_size > (0xFFFFFFFFFFFFFFFFULL >> 4U)) {
                break;
            }
            chunk_size = (chunk_size << 4U) | v;
            has_digit = 1;
        }

        if (has_digit == 0) {
            break;
        }

        in_off = line_end + 2ULL;
        if (chunk_size == 0ULL) {
            break;
        }

        if (in_off >= body_len) {
            break;
        }
        if (chunk_size > body_len - in_off) {
            chunk_size = body_len - in_off;
        }
        if (chunk_size == 0ULL) {
            break;
        }
        if (out_off + chunk_size + 1ULL > out_cap) {
            chunk_size = out_cap - 1ULL - out_off;
        }
        if (chunk_size == 0ULL) {
            break;
        }

        (void)memcpy(out + out_off, body + in_off, (usize)chunk_size);
        out_off += chunk_size;
        in_off += chunk_size;

        if (in_off + 1ULL >= body_len || body[in_off] != '\r' || body[in_off + 1ULL] != '\n') {
            break;
        }
        in_off += 2ULL;
    }

    if (out_off == 0ULL) {
        return 0;
    }

    out[out_off] = '\0';
    *out_len = out_off;
    return 1;
}
