#include "browser_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tls/cleonos_tls.h"

static char ush_browser_fetch_error[USH_BROWSER_FETCH_ERROR_MAX];

static void ush_browser_serial_log(const char *message) {
    if (message == (const char *)0 || message[0] == '\0') {
        return;
    }

    (void)cleonos_sys_log_write(message, ush_strlen(message));
}

static void ush_browser_fetch_error_set(const char *message) {
    if (message == (const char *)0 || message[0] == '\0') {
        ush_browser_fetch_error[0] = '\0';
        return;
    }

    ush_copy(ush_browser_fetch_error, (u64)sizeof(ush_browser_fetch_error), message);
}

static void ush_browser_fetch_error_set_tls(const char *prefix, const cleonos_tls_conn *conn) {
    char tls_error[96];
    char line[USH_BROWSER_FETCH_ERROR_MAX];
    char serial_line[USH_BROWSER_FETCH_ERROR_MAX + 48U];
    int tls_code;

    if (prefix == (const char *)0) {
        prefix = "tls failed";
    }

    tls_code = cleonos_tls_last_error(conn);
    cleonos_tls_error_text(tls_code, tls_error, (u64)sizeof(tls_error));
    if (snprintf(line, sizeof(line), "%s: %s (code=%d)", prefix, tls_error, tls_code) <= 0) {
        ush_browser_serial_log("[BROWSER][TLS] error formatting failed");
        ush_browser_fetch_error_set(prefix);
        return;
    }

    if (snprintf(serial_line, sizeof(serial_line), "[BROWSER][TLS] %s", line) > 0) {
        ush_browser_serial_log(serial_line);
    } else {
        ush_browser_serial_log("[BROWSER][TLS] error formatting failed");
    }
    ush_browser_fetch_error_set(line);
}

const char *ush_browser_fetch_last_error(void) {
    if (ush_browser_fetch_error[0] == '\0') {
        return "unknown fetch error";
    }
    return ush_browser_fetch_error;
}

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

static u16 ush_browser_read_be16(const u8 *ptr) {
    return (u16)(((u16)ptr[0] << 8U) | (u16)ptr[1]);
}

static void ush_browser_write_be16(u8 *ptr, u16 value) {
    ptr[0] = (u8)((value >> 8U) & 0xFFU);
    ptr[1] = (u8)(value & 0xFFU);
}

static int ush_browser_parse_ipv4_be(const char *text, u64 *out_ipv4_be) {
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

static int ush_browser_dns_encode_qname(const char *host, u8 *out, u64 out_cap, u64 *out_len) {
    const char *p;
    u64 at = 0ULL;

    if (host == (const char *)0 || out == (u8 *)0 || out_len == (u64 *)0 || out_cap < 2ULL || host[0] == '\0') {
        return 0;
    }

    p = host;
    while (*p != '\0') {
        const char *dot = p;
        u64 label_len;
        u64 i;

        while (*dot != '\0' && *dot != '.') {
            dot++;
        }

        label_len = (u64)(dot - p);
        if (label_len == 0ULL || label_len > 63ULL) {
            return 0;
        }

        if (at + 1ULL + label_len + 1ULL > out_cap) {
            return 0;
        }

        out[at++] = (u8)label_len;
        for (i = 0ULL; i < label_len; i++) {
            out[at++] = (u8)p[i];
        }

        if (*dot == '.') {
            p = dot + 1;
        } else {
            p = dot;
        }
    }

    out[at++] = 0U;
    *out_len = at;
    return 1;
}

static int ush_browser_dns_skip_name(const u8 *msg, u64 msg_len, u64 *io_off) {
    u64 pos;
    u64 depth = 0ULL;
    int jumped = 0;

    if (msg == (const u8 *)0 || io_off == (u64 *)0 || *io_off >= msg_len) {
        return 0;
    }

    pos = *io_off;
    while (depth < 32ULL) {
        u8 len;

        if (pos >= msg_len) {
            return 0;
        }

        len = msg[pos];
        if (len == 0U) {
            if (jumped == 0) {
                *io_off = pos + 1ULL;
            }
            return 1;
        }

        if ((len & 0xC0U) == 0xC0U) {
            u16 ptr;

            if (pos + 1ULL >= msg_len) {
                return 0;
            }

            ptr = (u16)(((u16)(len & 0x3FU) << 8U) | (u16)msg[pos + 1ULL]);
            if ((u64)ptr >= msg_len) {
                return 0;
            }

            if (jumped == 0) {
                *io_off = pos + 2ULL;
                jumped = 1;
            }

            pos = (u64)ptr;
            depth++;
            continue;
        }

        if ((len & 0xC0U) != 0U) {
            return 0;
        }

        if (pos + 1ULL + (u64)len > msg_len) {
            return 0;
        }

        pos += 1ULL + (u64)len;
        if (jumped == 0) {
            *io_off = pos;
        }
        depth++;
    }

    return 0;
}

static int ush_browser_dns_parse_first_a(const u8 *resp, u64 resp_len, u16 expected_id, u64 *out_ipv4_be) {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 i;
    u64 off = 12ULL;

    if (resp == (const u8 *)0 || out_ipv4_be == (u64 *)0 || resp_len < 12ULL) {
        return 0;
    }

    id = ush_browser_read_be16(&resp[0]);
    if (id != expected_id) {
        return 0;
    }

    flags = ush_browser_read_be16(&resp[2]);
    qdcount = ush_browser_read_be16(&resp[4]);
    ancount = ush_browser_read_be16(&resp[6]);

    if ((flags & 0x8000U) == 0U) {
        return 0;
    }

    if ((flags & 0x000FU) != 0U) {
        return 0;
    }

    for (i = 0U; i < qdcount; i++) {
        if (ush_browser_dns_skip_name(resp, resp_len, &off) == 0) {
            return 0;
        }
        if (off + 4ULL > resp_len) {
            return 0;
        }
        off += 4ULL;
    }

    for (i = 0U; i < ancount; i++) {
        u16 type;
        u16 klass;
        u16 rdlen;

        if (ush_browser_dns_skip_name(resp, resp_len, &off) == 0) {
            return 0;
        }
        if (off + 10ULL > resp_len) {
            return 0;
        }

        type = ush_browser_read_be16(&resp[off + 0ULL]);
        klass = ush_browser_read_be16(&resp[off + 2ULL]);
        rdlen = ush_browser_read_be16(&resp[off + 8ULL]);
        off += 10ULL;

        if (off + (u64)rdlen > resp_len) {
            return 0;
        }

        if (type == 1U && klass == 1U && rdlen == 4U) {
            *out_ipv4_be = ((u64)resp[off] << 24ULL) | ((u64)resp[off + 1ULL] << 16ULL) |
                           ((u64)resp[off + 2ULL] << 8ULL) | (u64)resp[off + 3ULL];
            return 1;
        }

        off += (u64)rdlen;
    }

    return 0;
}

static int ush_browser_dns_resolve_ipv4(const char *host, u64 *out_ipv4_be) {
    u64 dns_server;
    u8 query[USH_BROWSER_DNS_PACKET_MAX];
    u8 answer[USH_BROWSER_DNS_PACKET_MAX];
    u64 qname_len = 0ULL;
    u64 query_len;
    u16 txid;
    u16 src_port;
    cleonos_net_udp_send_req send_req;
    u64 loops;

    if (host == (const char *)0 || out_ipv4_be == (u64 *)0 || host[0] == '\0') {
        return 0;
    }

    dns_server = cleonos_sys_net_dns_server();
    if (dns_server == 0ULL) {
        return 0;
    }

    txid = (u16)((cleonos_sys_timer_ticks() ^ 0x4A31ULL) & 0xFFFFULL);
    src_port = (u16)(43000U + (txid % 20000U));

    (void)memset(query, 0, sizeof(query));
    if (ush_browser_dns_encode_qname(host, query + 12ULL, (u64)sizeof(query) - 12ULL, &qname_len) == 0) {
        return 0;
    }

    ush_browser_write_be16(&query[0], txid);
    ush_browser_write_be16(&query[2], 0x0100U);
    ush_browser_write_be16(&query[4], 1U);
    ush_browser_write_be16(&query[6], 0U);
    ush_browser_write_be16(&query[8], 0U);
    ush_browser_write_be16(&query[10], 0U);

    query_len = 12ULL + qname_len;
    if (query_len + 4ULL > (u64)sizeof(query)) {
        return 0;
    }

    ush_browser_write_be16(&query[query_len], 1U);
    ush_browser_write_be16(&query[query_len + 2ULL], 1U);
    query_len += 4ULL;

    send_req.dst_ipv4_be = dns_server;
    send_req.dst_port = 53ULL;
    send_req.src_port = (u64)src_port;
    send_req.payload_ptr = (u64)(usize)query;
    send_req.payload_len = query_len;

    if (cleonos_sys_net_udp_send(&send_req) != query_len) {
        return 0;
    }

    for (loops = 0ULL; loops < 700ULL; loops++) {
        cleonos_net_udp_recv_req recv_req;
        u64 got;
        u64 src_ip = 0ULL;
        u64 src_p = 0ULL;
        u64 dst_p = 0ULL;

        recv_req.out_payload_ptr = (u64)(usize)answer;
        recv_req.payload_capacity = (u64)sizeof(answer);
        recv_req.out_src_ipv4_ptr = (u64)(usize)&src_ip;
        recv_req.out_src_port_ptr = (u64)(usize)&src_p;
        recv_req.out_dst_port_ptr = (u64)(usize)&dst_p;

        got = cleonos_sys_net_udp_recv(&recv_req);
        if (got == 0ULL) {
            (void)cleonos_sys_sleep_ticks(2ULL);
            continue;
        }

        if (src_p != 53ULL || src_ip != dns_server) {
            continue;
        }

        if (ush_browser_dns_parse_first_a(answer, got, txid, out_ipv4_be) != 0) {
            return 1;
        }
    }

    return 0;
}

static char ush_browser_ascii_tolower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

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

static int ush_browser_line_has_token_icase(const char *line, u64 line_len, const char *token) {
    u64 token_len;
    u64 i;

    if (line == (const char *)0 || token == (const char *)0) {
        return 0;
    }

    token_len = ush_strlen(token);
    if (token_len == 0ULL || token_len > line_len) {
        return 0;
    }

    for (i = 0ULL; i + token_len <= line_len; i++) {
        u64 j;
        int match = 1;
        for (j = 0ULL; j < token_len; j++) {
            if (ush_browser_ascii_tolower(line[i + j]) != ush_browser_ascii_tolower(token[j])) {
                match = 0;
                break;
            }
        }
        if (match != 0) {
            return 1;
        }
    }

    return 0;
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

static int ush_browser_http_status_code(const char *raw, u64 raw_len) {
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

static int ush_browser_copy_http_header_value(const char *raw, u64 raw_len, const char *name, char *out, u64 out_cap) {
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

static int ush_browser_parse_http_headers(const char *raw, u64 raw_len, u64 *out_body_off, int *out_chunked,
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

static int ush_browser_find_http_header_end(const char *raw, u64 raw_len, u64 *out_body_off) {
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
static int ush_browser_is_chunked_body_complete(const char *body, u64 body_len) {
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

static int ush_browser_decode_chunked_body(const char *body, u64 body_len, char *out, u64 out_cap, u64 *out_len) {
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

int ush_browser_fetch_http(const char *url_text, char *out_html, u64 out_html_cap, u64 *out_size) {
    ush_browser_url url;
    u64 dst_ipv4_be = 0ULL;
    cleonos_net_tcp_connect_req conn_req;
    cleonos_net_tcp_send_req send_req;
    cleonos_tls_conn tls_conn;
    char request[1024];
    int request_len;
    u64 sent;
    u64 raw_len = 0ULL;
    u64 idle_loops = 0ULL;
    int tcp_open = 0;
    int tls_open = 0;
    int ok = 0;
    u64 body_off = 0ULL;
    int is_chunked = 0;
    u64 content_length = 0ULL;
    int has_content_length = 0;
    int is_compressed = 0;
    int header_parsed = 0;
    int status_code = 0;
    char redirect_location[USH_BROWSER_SOURCE_MAX];

    if (url_text == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0 || out_html_cap == 0ULL) {
        ush_browser_fetch_error_set("invalid fetch arguments");
        return 0;
    }

    ush_browser_fetch_error_set("");
    *out_size = 0ULL;
    out_html[0] = '\0';
    redirect_location[0] = '\0';

    if (ush_browser_ensure_buffers() == 0) {
        ush_browser_fetch_error_set("browser buffer allocation failed");
        return 0;
    }
    ush_zero(ush_browser_http_raw_buf, (u64)USH_BROWSER_HTML_BUF_CAP);

    if (cleonos_sys_net_available() == 0ULL) {
        ush_browser_fetch_error_set("network unavailable");
        return 0;
    }

    if (ush_browser_parse_url(url_text, &url) == 0) {
        ush_browser_fetch_error_set("invalid URL");
        return 0;
    }

    if (ush_browser_parse_ipv4_be(url.host, &dst_ipv4_be) == 0) {
        if (ush_browser_dns_resolve_ipv4(url.host, &dst_ipv4_be) == 0) {
            char line[USH_BROWSER_FETCH_ERROR_MAX];
            if (snprintf(line, sizeof(line), "DNS resolve failed for %s", url.host) > 0) {
                ush_browser_fetch_error_set(line);
            } else {
                ush_browser_fetch_error_set("DNS resolve failed");
            }
            return 0;
        }
    }

    ush_zero(&tls_conn, (u64)sizeof(tls_conn));
    if (url.tls != 0) {
        if (cleonos_tls_connect(&tls_conn, dst_ipv4_be, url.port, url.host, USH_BROWSER_TCP_POLL_BUDGET) == 0) {
            ush_browser_fetch_error_set_tls("TLS connect failed", &tls_conn);
            goto cleanup;
        }
        tls_open = 1;
    } else {
        ush_zero(&conn_req, (u64)sizeof(conn_req));
        conn_req.dst_ipv4_be = dst_ipv4_be;
        conn_req.dst_port = (u64)url.port;
        conn_req.src_port = 0ULL;
        conn_req.poll_budget = USH_BROWSER_TCP_POLL_BUDGET;

        if (cleonos_sys_net_tcp_connect(&conn_req) == 0ULL) {
            ush_browser_fetch_error_set("TCP connect failed");
            goto cleanup;
        }
        tcp_open = 1;
    }

    if (!((url.tls == 0 && url.port == 80U) || (url.tls != 0 && url.port == 443U))) {
        request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: cleonos-browser/1.0\r\nAccept: "
                               "text/html,*/*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
                               url.path, url.host, (unsigned int)url.port);
    } else {
        request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: cleonos-browser/1.0\r\nAccept: "
                               "text/html,*/*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
                               url.path, url.host);
    }

    if (request_len <= 0 || (u64)request_len >= (u64)sizeof(request)) {
        ush_browser_fetch_error_set("HTTP request build failed");
        goto cleanup;
    }

    if (url.tls != 0) {
        if (cleonos_tls_write_all(&tls_conn, request, (u64)request_len) == 0) {
            ush_browser_fetch_error_set_tls("TLS send failed", &tls_conn);
            goto cleanup;
        }
    } else {
        send_req.payload_ptr = (u64)(usize)request;
        send_req.payload_len = (u64)request_len;
        send_req.poll_budget = USH_BROWSER_TCP_POLL_BUDGET;

        sent = cleonos_sys_net_tcp_send(&send_req);
        if (sent != (u64)request_len) {
            ush_browser_fetch_error_set("TCP send failed");
            goto cleanup;
        }
    }

    while (raw_len + 1ULL < (u64)USH_BROWSER_HTML_BUF_CAP) {
        u8 chunk[USH_BROWSER_HTTP_RECV_CHUNK];
        u64 got = 0ULL;
        u64 cap_left = (u64)USH_BROWSER_HTML_BUF_CAP - 1ULL - raw_len;

        if (url.tls != 0) {
            int tls_got = cleonos_tls_read(&tls_conn, chunk, (u64)sizeof(chunk));
            if (tls_got < 0) {
                ush_browser_fetch_error_set_tls("TLS recv failed", &tls_conn);
                goto cleanup;
            }
            got = (u64)tls_got;
        } else {
            cleonos_net_tcp_recv_req recv_req;
            recv_req.out_payload_ptr = (u64)(usize)chunk;
            recv_req.payload_capacity = (u64)sizeof(chunk);
            recv_req.poll_budget = 60000ULL;

            got = cleonos_sys_net_tcp_recv(&recv_req);
        }

        if (got == (u64)-1) {
            ush_browser_fetch_error_set("TCP recv failed");
            goto cleanup;
        }

        if (got == 0ULL) {
            if (url.tls != 0 && cleonos_tls_eof(&tls_conn) != 0) {
                break;
            }
            if (header_parsed != 0) {
                if (is_chunked == 0 && has_content_length != 0 && raw_len >= body_off + content_length) {
                    break;
                }
                if (is_chunked != 0) {
                    int complete =
                        ush_browser_is_chunked_body_complete(ush_browser_http_raw_buf + body_off, raw_len - body_off);
                    if (complete < 0) {
                        ush_browser_fetch_error_set("invalid chunked HTTP response");
                        goto cleanup;
                    }
                    if (complete > 0) {
                        break;
                    }
                }
            }

            idle_loops++;
            if (idle_loops >= USH_BROWSER_TCP_RECV_IDLE_LOOPS) {
                break;
            }
            (void)cleonos_sys_sleep_ticks(1ULL);
            continue;
        }

        idle_loops = 0ULL;
        if (got > cap_left) {
            got = cap_left;
        }
        (void)memcpy(ush_browser_http_raw_buf + raw_len, chunk, (usize)got);
        raw_len += got;

        if (header_parsed == 0) {
            u64 maybe_body_off = 0ULL;

            if (ush_browser_find_http_header_end(ush_browser_http_raw_buf, raw_len, &maybe_body_off) != 0) {
                if (ush_browser_parse_http_headers(ush_browser_http_raw_buf, raw_len, &body_off, &is_chunked,
                                                   &content_length, &has_content_length, &is_compressed) == 0) {
                    ush_browser_fetch_error_set("invalid HTTP headers");
                    goto cleanup;
                }

                header_parsed = 1;
                if (is_compressed != 0) {
                    ush_browser_fetch_error_set("compressed HTTP response is not supported");
                    goto cleanup;
                }
            }
        }

        if (header_parsed != 0) {
            if (is_chunked == 0 && has_content_length != 0 && raw_len >= body_off + content_length) {
                break;
            }
            if (is_chunked != 0) {
                int complete =
                    ush_browser_is_chunked_body_complete(ush_browser_http_raw_buf + body_off, raw_len - body_off);
                if (complete < 0) {
                    ush_browser_fetch_error_set("invalid chunked HTTP response");
                    goto cleanup;
                }
                if (complete > 0) {
                    break;
                }
            }
        }
    }

    ush_browser_http_raw_buf[raw_len] = '\0';
    if (raw_len == 0ULL) {
        ush_browser_fetch_error_set("empty HTTP response");
        goto cleanup;
    }

    if (header_parsed == 0) {
        if (ush_browser_parse_http_headers(ush_browser_http_raw_buf, raw_len, &body_off, &is_chunked, &content_length,
                                           &has_content_length, &is_compressed) == 0) {
            ush_browser_fetch_error_set("invalid HTTP headers");
            goto cleanup;
        }
        if (is_compressed != 0) {
            ush_browser_fetch_error_set("compressed HTTP response is not supported");
            goto cleanup;
        }
    }

    if (body_off > raw_len) {
        ush_browser_fetch_error_set("invalid HTTP body offset");
        goto cleanup;
    }

    status_code = ush_browser_http_status_code(ush_browser_http_raw_buf, raw_len);
    if (status_code >= 300 && status_code < 400 &&
        ush_browser_copy_http_header_value(ush_browser_http_raw_buf, raw_len, "Location", redirect_location,
                                           (u64)sizeof(redirect_location)) != 0 &&
        (raw_len == body_off || (has_content_length != 0 && content_length == 0ULL))) {
        int redirect_len =
            snprintf(out_html, (size_t)out_html_cap,
                     "<html><head><title>Redirect</title></head><body><h1>Redirect</h1><p>This page redirects to "
                     "<a href=\"%s\">%s</a>.</p></body></html>",
                     redirect_location, redirect_location);
        if (redirect_len <= 0) {
            ush_browser_fetch_error_set("redirect page build failed");
            goto cleanup;
        }
        if ((u64)redirect_len >= out_html_cap) {
            redirect_len = (int)out_html_cap - 1;
        }
        out_html[redirect_len] = '\0';
        *out_size = (u64)redirect_len;
        ok = 1;
        goto cleanup;
    }

    if (is_chunked != 0) {
        if (ush_browser_decode_chunked_body(ush_browser_http_raw_buf + body_off, raw_len - body_off, out_html,
                                            out_html_cap, out_size) == 0) {
            ush_browser_fetch_error_set("chunked HTTP body decode failed");
            goto cleanup;
        }
    } else {
        u64 copy_len = raw_len - body_off;
        if (has_content_length != 0 && content_length < copy_len) {
            copy_len = content_length;
        }
        if (copy_len + 1ULL > out_html_cap) {
            copy_len = out_html_cap - 1ULL;
        }
        (void)memcpy(out_html, ush_browser_http_raw_buf + body_off, (usize)copy_len);
        out_html[copy_len] = '\0';
        *out_size = copy_len;
    }

    ok = (*out_size > 0ULL) ? 1 : 0;
    if (ok == 0) {
        ush_browser_fetch_error_set("HTTP response body is empty");
    }

cleanup:
    if (tls_open != 0) {
        cleonos_tls_close(&tls_conn, USH_BROWSER_TCP_POLL_BUDGET);
    } else if (tcp_open != 0) {
        (void)cleonos_sys_net_tcp_close(USH_BROWSER_TCP_POLL_BUDGET);
    }
    if (ok == 0) {
        *out_size = 0ULL;
        out_html[0] = '\0';
    }
    return ok;
}
