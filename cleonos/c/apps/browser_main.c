#include "cmd_runtime.h"

#include <stdio.h>
#include <string.h>

#include "gumbo.h"

#define USH_BROWSER_SOURCE_MAX 256U
#define USH_BROWSER_HOST_MAX 128U
#define USH_BROWSER_PATH_MAX 256U
#define USH_BROWSER_HTML_MAX (512U * 1024U)
#define USH_BROWSER_TEXT_MAX (160U * 1024U)
#define USH_BROWSER_TITLE_MAX 128U
#define USH_BROWSER_DNS_PACKET_MAX 512U
#define USH_BROWSER_HTTP_RECV_CHUNK 2048U
#define USH_BROWSER_TCP_POLL_BUDGET 200000000ULL
#define USH_BROWSER_TCP_RECV_IDLE_LOOPS 40ULL

#define USH_BROWSER_LINK_MAX 128U
#define USH_BROWSER_LINK_TEXT_MAX 96U
#define USH_BROWSER_LINK_HREF_MAX 192U
#define USH_BROWSER_HISTORY_MAX 16U
#define USH_BROWSER_INPUT_MAX 256U
#define USH_BROWSER_SEG_MAX 32U
#define USH_BROWSER_SEG_LEN_MAX 63U
#define USH_BROWSER_CSS_TEXT_MAX 4096U
#define USH_BROWSER_ANSI_RESET "\x1B[0m"
#define USH_BROWSER_ANSI_BLUE "\x1B[34m"
#define USH_BROWSER_ANSI_UNDERLINE "\x1B[4m"
#define USH_BROWSER_ANSI_BLUE_UNDERLINE "\x1B[34;4m"

typedef unsigned char u8;
typedef unsigned short u16;

typedef struct ush_browser_url {
    char host[USH_BROWSER_HOST_MAX];
    char path[USH_BROWSER_PATH_MAX];
    u16 port;
} ush_browser_url;

typedef struct ush_browser_link {
    char text[USH_BROWSER_LINK_TEXT_MAX];
    char href[USH_BROWSER_LINK_HREF_MAX];
} ush_browser_link;

static char ush_browser_html_buf[USH_BROWSER_HTML_MAX + 1U];
static char ush_browser_http_raw_buf[USH_BROWSER_HTML_MAX + 1U];
static char ush_browser_text_buf[USH_BROWSER_TEXT_MAX + 1U];
static char ush_browser_title[USH_BROWSER_TITLE_MAX];
static ush_browser_link ush_browser_links[USH_BROWSER_LINK_MAX];

static u64 ush_browser_text_len = 0ULL;
static int ush_browser_last_space = 1;
static u64 ush_browser_link_count = 0ULL;
static int ush_browser_css_link_blue = 1;
static int ush_browser_css_link_underline = 1;

static int ush_browser_is_http_url(const char *text) {
    if (text == (const char *)0) {
        return 0;
    }

    return (text[0] == 'h' && text[1] == 't' && text[2] == 't' && text[3] == 'p' && text[4] == ':' &&
            text[5] == '/' && text[6] == '/')
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

static int ush_browser_parse_url(const char *url, ush_browser_url *out_url) {
    const char *p;
    const char *host_begin;
    const char *host_end;
    const char *path_begin;
    u64 host_len;
    u64 path_len;
    u64 i;

    if (url == (const char *)0 || out_url == (ush_browser_url *)0) {
        return 0;
    }

    ush_zero(out_url, (u64)sizeof(*out_url));

    if (url[0] == '\0') {
        return 0;
    }

    if (!(url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' && url[4] == ':' && url[5] == '/' &&
          url[6] == '/')) {
        return 0;
    }

    p = url + 7;
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

    out_url->port = 80U;
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

static int ush_browser_parse_http_headers(const char *raw, u64 raw_len, u64 *out_body_off, int *out_chunked, u64 *out_content_len,
                                          int *out_has_content_len, int *out_compressed) {
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
                return 0;
            }

            if (chunk_size > (0xFFFFFFFFFFFFFFFFULL >> 4U)) {
                return 0;
            }
            chunk_size = (chunk_size << 4U) | v;
            has_digit = 1;
        }

        if (has_digit == 0) {
            return 0;
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
                    in_off = line_end + 2ULL;
                    break;
                }

                in_off = line_end + 2ULL;
            }
            break;
        }

        if (in_off + chunk_size > body_len) {
            return 0;
        }
        if (out_off + chunk_size + 1ULL > out_cap) {
            return 0;
        }

        (void)memcpy(out + out_off, body + in_off, (usize)chunk_size);
        out_off += chunk_size;
        in_off += chunk_size;

        if (in_off + 1ULL >= body_len || body[in_off] != '\r' || body[in_off + 1ULL] != '\n') {
            return 0;
        }
        in_off += 2ULL;
    }

    out[out_off] = '\0';
    *out_len = out_off;
    return 1;
}

static int ush_browser_fetch_http(const char *url_text, char *out_html, u64 out_html_cap, u64 *out_size) {
    ush_browser_url url;
    u64 dst_ipv4_be = 0ULL;
    cleonos_net_tcp_connect_req conn_req;
    cleonos_net_tcp_send_req send_req;
    char request[1024];
    int request_len;
    u64 sent;
    u64 raw_len = 0ULL;
    u64 idle_loops = 0ULL;
    int connected = 0;
    int ok = 0;
    u64 body_off = 0ULL;
    int is_chunked = 0;
    u64 content_length = 0ULL;
    int has_content_length = 0;
    int is_compressed = 0;
    int header_parsed = 0;

    if (url_text == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0 || out_html_cap == 0ULL) {
        return 0;
    }

    *out_size = 0ULL;
    out_html[0] = '\0';
    ush_zero(ush_browser_http_raw_buf, (u64)sizeof(ush_browser_http_raw_buf));

    if (cleonos_sys_net_available() == 0ULL) {
        return 0;
    }

    if (ush_browser_parse_url(url_text, &url) == 0) {
        return 0;
    }

    if (ush_browser_parse_ipv4_be(url.host, &dst_ipv4_be) == 0) {
        if (ush_browser_dns_resolve_ipv4(url.host, &dst_ipv4_be) == 0) {
            return 0;
        }
    }

    ush_zero(&conn_req, (u64)sizeof(conn_req));
    conn_req.dst_ipv4_be = dst_ipv4_be;
    conn_req.dst_port = (u64)url.port;
    conn_req.src_port = 0ULL;
    conn_req.poll_budget = USH_BROWSER_TCP_POLL_BUDGET;

    if (cleonos_sys_net_tcp_connect(&conn_req) == 0ULL) {
        return 0;
    }
    connected = 1;

    if (url.port != 80U) {
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
        goto cleanup;
    }

    send_req.payload_ptr = (u64)(usize)request;
    send_req.payload_len = (u64)request_len;
    send_req.poll_budget = USH_BROWSER_TCP_POLL_BUDGET;

    sent = cleonos_sys_net_tcp_send(&send_req);
    if (sent != (u64)request_len) {
        goto cleanup;
    }

    while (raw_len + 1ULL < (u64)sizeof(ush_browser_http_raw_buf)) {
        cleonos_net_tcp_recv_req recv_req;
        u8 chunk[USH_BROWSER_HTTP_RECV_CHUNK];
        u64 got;
        u64 cap_left = (u64)sizeof(ush_browser_http_raw_buf) - 1ULL - raw_len;

        recv_req.out_payload_ptr = (u64)(usize)chunk;
        recv_req.payload_capacity = (u64)sizeof(chunk);
        recv_req.poll_budget = 60000ULL;

        got = cleonos_sys_net_tcp_recv(&recv_req);
        if (got == 0ULL) {
            if (header_parsed != 0) {
                if (is_chunked == 0 && has_content_length != 0 && raw_len >= body_off + content_length) {
                    break;
                }
                if (is_chunked != 0) {
                    int complete = ush_browser_is_chunked_body_complete(ush_browser_http_raw_buf + body_off, raw_len - body_off);
                    if (complete < 0) {
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
                if (ush_browser_parse_http_headers(ush_browser_http_raw_buf, raw_len, &body_off, &is_chunked, &content_length,
                                                   &has_content_length, &is_compressed) == 0) {
                    goto cleanup;
                }

                header_parsed = 1;
                if (is_compressed != 0) {
                    goto cleanup;
                }
            }
        }

        if (header_parsed != 0) {
            if (is_chunked == 0 && has_content_length != 0 && raw_len >= body_off + content_length) {
                break;
            }
            if (is_chunked != 0) {
                int complete = ush_browser_is_chunked_body_complete(ush_browser_http_raw_buf + body_off, raw_len - body_off);
                if (complete < 0) {
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
        goto cleanup;
    }

    if (header_parsed == 0) {
        if (ush_browser_parse_http_headers(ush_browser_http_raw_buf, raw_len, &body_off, &is_chunked, &content_length,
                                           &has_content_length, &is_compressed) == 0) {
            goto cleanup;
        }
        if (is_compressed != 0) {
            goto cleanup;
        }
    }

    if (body_off > raw_len) {
        goto cleanup;
    }

    if (is_chunked != 0) {
        if (ush_browser_decode_chunked_body(ush_browser_http_raw_buf + body_off, raw_len - body_off, out_html, out_html_cap,
                                            out_size) == 0) {
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

cleanup:
    if (connected != 0) {
        (void)cleonos_sys_net_tcp_close(USH_BROWSER_TCP_POLL_BUDGET);
    }
    if (ok == 0) {
        *out_size = 0ULL;
        out_html[0] = '\0';
    }
    return ok;
}

static void ush_browser_text_reset(void) {
    ush_browser_text_len = 0ULL;
    ush_browser_text_buf[0] = '\0';
    ush_browser_last_space = 1;
}

static void ush_browser_text_trim_trailing_spaces(void) {
    while (ush_browser_text_len > 0ULL && ush_browser_text_buf[ush_browser_text_len - 1ULL] == ' ') {
        ush_browser_text_len--;
    }
    ush_browser_text_buf[ush_browser_text_len] = '\0';
}

static void ush_browser_text_newline(void) {
    if (ush_browser_text_len == 0ULL) {
        return;
    }

    ush_browser_text_trim_trailing_spaces();
    if (ush_browser_text_len == 0ULL) {
        return;
    }

    if (ush_browser_text_buf[ush_browser_text_len - 1ULL] == '\n') {
        if (ush_browser_text_len >= 2ULL && ush_browser_text_buf[ush_browser_text_len - 2ULL] == '\n') {
            return;
        }
    }

    if (ush_browser_text_len + 1ULL >= (u64)sizeof(ush_browser_text_buf)) {
        return;
    }

    ush_browser_text_buf[ush_browser_text_len++] = '\n';
    ush_browser_text_buf[ush_browser_text_len] = '\0';
    ush_browser_last_space = 1;
}

static void ush_browser_text_append_char(char ch) {
    if (ush_browser_text_len + 1ULL >= (u64)sizeof(ush_browser_text_buf)) {
        return;
    }

    if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\v') {
        ch = ' ';
    }

    if ((unsigned char)ch < 0x20U) {
        return;
    }

    if (ch == ' ') {
        if (ush_browser_last_space != 0) {
            return;
        }
        ush_browser_last_space = 1;
    } else {
        ush_browser_last_space = 0;
    }

    ush_browser_text_buf[ush_browser_text_len++] = ch;
    ush_browser_text_buf[ush_browser_text_len] = '\0';
}

static void ush_browser_text_append(const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        ush_browser_text_append_char(text[i]);
        i++;
    }
}

static void ush_browser_text_append_raw(const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        if (ush_browser_text_len + 1ULL >= (u64)sizeof(ush_browser_text_buf)) {
            return;
        }

        ush_browser_text_buf[ush_browser_text_len++] = text[i++];
        ush_browser_text_buf[ush_browser_text_len] = '\0';
    }
}

static void ush_browser_text_begin_link_style(int blue, int underline) {
    if (blue != 0 && underline != 0) {
        ush_browser_text_append_raw(USH_BROWSER_ANSI_BLUE_UNDERLINE);
    } else if (blue != 0) {
        ush_browser_text_append_raw(USH_BROWSER_ANSI_BLUE);
    } else if (underline != 0) {
        ush_browser_text_append_raw(USH_BROWSER_ANSI_UNDERLINE);
    }
}

static void ush_browser_text_end_link_style(void) {
    ush_browser_text_append_raw(USH_BROWSER_ANSI_RESET);
}

static int ush_browser_css_is_space(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v') ? 1 : 0;
}

static int ush_browser_css_name_eq(const char *name, u64 name_len, const char *lit) {
    u64 lit_len = ush_strlen(lit);
    u64 i;

    if (name == (const char *)0 || lit == (const char *)0 || name_len != lit_len) {
        return 0;
    }

    for (i = 0ULL; i < lit_len; i++) {
        if (ush_browser_ascii_tolower(name[i]) != ush_browser_ascii_tolower(lit[i])) {
            return 0;
        }
    }

    return 1;
}

static int ush_browser_css_color_is_blue(const char *value, u64 value_len) {
    char normalized[64];
    u64 at = 0ULL;
    u64 i;

    if (value == (const char *)0) {
        return 0;
    }

    for (i = 0ULL; i < value_len; i++) {
        char ch = value[i];

        if (ush_browser_css_is_space(ch) != 0) {
            continue;
        }
        if (at + 1ULL >= (u64)sizeof(normalized)) {
            break;
        }
        normalized[at++] = ush_browser_ascii_tolower(ch);
    }
    normalized[at] = '\0';

    if (ush_streq(normalized, "blue") != 0 || ush_streq(normalized, "#00f") != 0 || ush_streq(normalized, "#0000ff") != 0 ||
        ush_streq(normalized, "rgb(0,0,255)") != 0) {
        return 1;
    }

    return 0;
}

static void ush_browser_css_apply_declarations(const char *decl, u64 decl_len, int *io_link_blue, int *io_link_underline) {
    u64 pos = 0ULL;

    if (decl == (const char *)0 || io_link_blue == (int *)0 || io_link_underline == (int *)0) {
        return;
    }

    while (pos < decl_len) {
        u64 name_start;
        u64 name_end;
        u64 value_start;
        u64 value_end;

        while (pos < decl_len && (ush_browser_css_is_space(decl[pos]) != 0 || decl[pos] == ';')) {
            pos++;
        }
        if (pos >= decl_len) {
            break;
        }

        name_start = pos;
        while (pos < decl_len && decl[pos] != ':' && decl[pos] != ';' && decl[pos] != '}') {
            pos++;
        }
        name_end = pos;
        while (name_end > name_start && ush_browser_css_is_space(decl[name_end - 1ULL]) != 0) {
            name_end--;
        }

        if (pos >= decl_len || decl[pos] != ':') {
            while (pos < decl_len && decl[pos] != ';') {
                pos++;
            }
            if (pos < decl_len) {
                pos++;
            }
            continue;
        }

        pos++;
        value_start = pos;
        while (pos < decl_len && decl[pos] != ';' && decl[pos] != '}') {
            pos++;
        }
        value_end = pos;
        while (value_start < value_end && ush_browser_css_is_space(decl[value_start]) != 0) {
            value_start++;
        }
        while (value_end > value_start && ush_browser_css_is_space(decl[value_end - 1ULL]) != 0) {
            value_end--;
        }

        if (name_end > name_start && value_end >= value_start) {
            if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "text-decoration") != 0) {
                if (ush_browser_line_has_token_icase(decl + value_start, value_end - value_start, "none") != 0) {
                    *io_link_underline = 0;
                }
                if (ush_browser_line_has_token_icase(decl + value_start, value_end - value_start, "underline") != 0) {
                    *io_link_underline = 1;
                }
            } else if (ush_browser_css_name_eq(decl + name_start, name_end - name_start, "color") != 0) {
                if (ush_browser_line_has_token_icase(decl + value_start, value_end - value_start, "inherit") != 0 ||
                    ush_browser_line_has_token_icase(decl + value_start, value_end - value_start, "initial") != 0 ||
                    ush_browser_line_has_token_icase(decl + value_start, value_end - value_start, "unset") != 0) {
                    /* keep previous style */
                } else {
                    *io_link_blue = (ush_browser_css_color_is_blue(decl + value_start, value_end - value_start) != 0) ? 1 : 0;
                }
            }
        }

        if (pos < decl_len && decl[pos] == ';') {
            pos++;
        }
    }
}

static int ush_browser_css_selector_targets_anchor(const char *selector, u64 selector_len) {
    u64 i;

    if (selector == (const char *)0) {
        return 0;
    }

    for (i = 0ULL; i < selector_len; i++) {
        char ch = ush_browser_ascii_tolower(selector[i]);
        if (ch == 'a') {
            char prev = (i == 0ULL) ? ' ' : selector[i - 1ULL];
            char next = (i + 1ULL < selector_len) ? selector[i + 1ULL] : ' ';
            int prev_ok = (i == 0ULL || prev == ' ' || prev == '\t' || prev == '\r' || prev == '\n' || prev == ',' ||
                           prev == '>' || prev == '+' || prev == '~')
                              ? 1
                              : 0;
            int next_ok = (next == ' ' || next == '\t' || next == '\r' || next == '\n' || next == ',' || next == '\0' ||
                           next == '.' || next == '#' || next == ':' || next == '[' || next == '>' || next == '+' ||
                           next == '~')
                              ? 1
                              : 0;

            if (prev_ok != 0 && next_ok != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static void ush_browser_css_parse_stylesheet(const char *css_text) {
    u64 pos = 0ULL;
    u64 css_len;

    if (css_text == (const char *)0) {
        return;
    }

    css_len = ush_strlen(css_text);
    while (pos < css_len) {
        u64 selector_start = pos;
        u64 selector_end;
        u64 decl_start;
        u64 decl_end;

        while (pos < css_len && css_text[pos] != '{') {
            pos++;
        }
        if (pos >= css_len) {
            break;
        }

        selector_end = pos;
        pos++;
        decl_start = pos;
        while (pos < css_len && css_text[pos] != '}') {
            pos++;
        }
        decl_end = pos;

        if (selector_end > selector_start &&
            ush_browser_css_selector_targets_anchor(css_text + selector_start, selector_end - selector_start) != 0) {
            ush_browser_css_apply_declarations(css_text + decl_start, decl_end - decl_start, &ush_browser_css_link_blue,
                                               &ush_browser_css_link_underline);
        }

        if (pos < css_len && css_text[pos] == '}') {
            pos++;
        }
    }
}

static void ush_browser_collect_style_text(GumboNode *node, char *out, u64 out_cap, u64 *io_len) {
    if (node == (GumboNode *)0 || out == (char *)0 || io_len == (u64 *)0 || out_cap == 0ULL) {
        return;
    }

    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
        const char *txt = node->v.text.text;
        u64 i = 0ULL;

        while (txt != (const char *)0 && txt[i] != '\0') {
            if (*io_len + 1ULL >= out_cap) {
                break;
            }
            out[(*io_len)++] = txt[i++];
        }
        out[*io_len] = '\0';
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children = (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_collect_style_text((GumboNode *)children->data[i], out, out_cap, io_len);
        }
    }
}

static void ush_browser_css_scan_style_nodes(GumboNode *node) {
    if (node == (GumboNode *)0) {
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag == GUMBO_TAG_STYLE) {
        char css_text[USH_BROWSER_CSS_TEXT_MAX];
        u64 css_len = 0ULL;

        ush_zero(css_text, (u64)sizeof(css_text));
        ush_browser_collect_style_text(node, css_text, (u64)sizeof(css_text), &css_len);
        if (css_len > 0ULL) {
            ush_browser_css_parse_stylesheet(css_text);
        }
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children = (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_css_scan_style_nodes((GumboNode *)children->data[i]);
        }
    }
}

static int ush_browser_is_skip_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_SCRIPT:
    case GUMBO_TAG_STYLE:
    case GUMBO_TAG_NOSCRIPT:
    case GUMBO_TAG_TEMPLATE:
    case GUMBO_TAG_IFRAME:
    case GUMBO_TAG_OBJECT:
        return 1;
    default:
        return 0;
    }
}

static int ush_browser_is_block_tag(GumboTag tag) {
    switch (tag) {
    case GUMBO_TAG_HTML:
    case GUMBO_TAG_HEAD:
    case GUMBO_TAG_BODY:
    case GUMBO_TAG_DIV:
    case GUMBO_TAG_SECTION:
    case GUMBO_TAG_ARTICLE:
    case GUMBO_TAG_ASIDE:
    case GUMBO_TAG_NAV:
    case GUMBO_TAG_P:
    case GUMBO_TAG_UL:
    case GUMBO_TAG_OL:
    case GUMBO_TAG_LI:
    case GUMBO_TAG_TABLE:
    case GUMBO_TAG_TR:
    case GUMBO_TAG_TD:
    case GUMBO_TAG_TH:
    case GUMBO_TAG_PRE:
    case GUMBO_TAG_BLOCKQUOTE:
    case GUMBO_TAG_H1:
    case GUMBO_TAG_H2:
    case GUMBO_TAG_H3:
    case GUMBO_TAG_H4:
    case GUMBO_TAG_H5:
    case GUMBO_TAG_H6:
    case GUMBO_TAG_FOOTER:
    case GUMBO_TAG_HEADER:
    case GUMBO_TAG_MAIN:
        return 1;
    default:
        return 0;
    }
}

static void ush_browser_collect_plain_text(GumboNode *node, char *out, u64 out_cap, u64 *io_len) {
    if (node == (GumboNode *)0 || out == (char *)0 || io_len == (u64 *)0 || out_cap == 0ULL) {
        return;
    }

    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
        const char *txt = node->v.text.text;
        u64 i = 0ULL;

        while (txt != (const char *)0 && txt[i] != '\0') {
            char ch = txt[i];

            if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\v') {
                ch = ' ';
            }

            if ((unsigned char)ch >= 0x20U) {
                if (*io_len + 1ULL >= out_cap) {
                    break;
                }

                if (ch == ' ') {
                    if (*io_len > 0ULL && out[*io_len - 1ULL] != ' ') {
                        out[(*io_len)++] = ' ';
                    }
                } else {
                    out[(*io_len)++] = ch;
                }
            }

            i++;
        }

        out[*io_len] = '\0';
        return;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        GumboVector *children = &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_collect_plain_text((GumboNode *)children->data[i], out, out_cap, io_len);
        }
    }
}

static void ush_browser_collect_anchor_link(GumboNode *node) {
    GumboAttribute *href;
    u64 text_len = 0ULL;
    char text[USH_BROWSER_LINK_TEXT_MAX];

    if (node == (GumboNode *)0 || node->type != GUMBO_NODE_ELEMENT || node->v.element.tag != GUMBO_TAG_A) {
        return;
    }

    href = gumbo_get_attribute(&node->v.element.attributes, "href");
    if (href == (GumboAttribute *)0 || href->value == (const char *)0 || href->value[0] == '\0') {
        return;
    }

    if (ush_browser_link_count >= (u64)USH_BROWSER_LINK_MAX) {
        return;
    }

    ush_zero(text, (u64)sizeof(text));
    ush_browser_collect_plain_text(node, text, (u64)sizeof(text), &text_len);
    if (text[0] == '\0') {
        ush_copy(text, (u64)sizeof(text), "(link)");
    }

    ush_copy(ush_browser_links[ush_browser_link_count].text, (u64)sizeof(ush_browser_links[ush_browser_link_count].text),
             text);
    ush_copy(ush_browser_links[ush_browser_link_count].href, (u64)sizeof(ush_browser_links[ush_browser_link_count].href),
             href->value);
    ush_browser_link_count++;
}

static int ush_browser_find_title_node(GumboNode *node, char *out, u64 out_cap) {
    if (node == (GumboNode *)0 || out == (char *)0 || out_cap == 0ULL) {
        return 0;
    }

    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag == GUMBO_TAG_TITLE) {
        u64 len = 0ULL;
        out[0] = '\0';
        ush_browser_collect_plain_text(node, out, out_cap, &len);
        return (out[0] != '\0') ? 1 : 0;
    }

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE || node->type == GUMBO_NODE_DOCUMENT) {
        GumboVector *children = (node->type == GUMBO_NODE_DOCUMENT) ? &node->v.document.children : &node->v.element.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            if (ush_browser_find_title_node((GumboNode *)children->data[i], out, out_cap) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static void ush_browser_walk_dom(GumboNode *node) {
    if (node == (GumboNode *)0) {
        return;
    }

    switch (node->type) {
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_WHITESPACE:
    case GUMBO_NODE_CDATA:
        ush_browser_text_append(node->v.text.text);
        return;
    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE: {
        GumboTag tag = node->v.element.tag;
        GumboVector *children = &node->v.element.children;
        int is_block = ush_browser_is_block_tag(tag);
        int is_anchor = (tag == GUMBO_TAG_A) ? 1 : 0;
        int link_blue = ush_browser_css_link_blue;
        int link_underline = ush_browser_css_link_underline;
        u64 i;

        if (ush_browser_is_skip_tag(tag) != 0) {
            return;
        }

        if (tag == GUMBO_TAG_BR || tag == GUMBO_TAG_HR) {
            ush_browser_text_newline();
            return;
        }

        if (is_block != 0) {
            ush_browser_text_newline();
            if (tag == GUMBO_TAG_LI) {
                ush_browser_text_append("* ");
            }
        }

        if (is_anchor != 0) {
            GumboAttribute *style_attr;

            ush_browser_collect_anchor_link(node);
            style_attr = gumbo_get_attribute(&node->v.element.attributes, "style");
            if (style_attr != (GumboAttribute *)0 && style_attr->value != (const char *)0) {
                ush_browser_css_apply_declarations(style_attr->value, ush_strlen(style_attr->value), &link_blue,
                                                   &link_underline);
            }
            ush_browser_text_begin_link_style(link_blue, link_underline);
        }

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom((GumboNode *)children->data[i]);
        }

        if (is_anchor != 0) {
            ush_browser_text_end_link_style();
        }

        if (is_block != 0) {
            ush_browser_text_newline();
        }
        return;
    }
    case GUMBO_NODE_DOCUMENT: {
        GumboVector *children = &node->v.document.children;
        u64 i;

        for (i = 0ULL; i < (u64)children->length; i++) {
            ush_browser_walk_dom((GumboNode *)children->data[i]);
        }
        return;
    }
    default:
        return;
    }
}

static int ush_browser_read_file(const ush_state *sh, const char *arg, char *out_html, u64 out_html_cap, u64 *out_size) {
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

static int ush_browser_render_html(const char *html, u64 html_size) {
    GumboOutput *output;

    if (html == (const char *)0 || html_size == 0ULL) {
        return 0;
    }

    output = gumbo_parse_with_options(&kGumboDefaultOptions, html, (size_t)html_size);
    if (output == (GumboOutput *)0 || output->root == (GumboNode *)0) {
        return 0;
    }

    ush_browser_link_count = 0ULL;
    ush_browser_text_reset();
    ush_zero(ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_css_link_blue = 1;
    ush_browser_css_link_underline = 1;

    (void)ush_browser_find_title_node(output->root, ush_browser_title, (u64)sizeof(ush_browser_title));
    ush_browser_css_scan_style_nodes(output->root);
    ush_browser_walk_dom(output->root);
    ush_browser_text_trim_trailing_spaces();

    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return 1;
}

static void ush_browser_print_rendered(const char *source_desc) {
    u64 i = 0ULL;
    u64 line_count = 0ULL;
    u64 k;

    /* 3J clears TTY scrollback, 2J clears screen, H moves cursor home. */
    ush_writeln("\x1B[3J\x1B[2J\x1B[H");
    ush_writeln("[browser] litehtml-gumbo text renderer");
    (void)printf("[browser] source: %s\n", source_desc);
    if (ush_browser_title[0] != '\0') {
        (void)printf("[browser] title: %s\n", ush_browser_title);
    } else {
        ush_writeln("[browser] title: (none)");
    }
    ush_writeln("------------------------------------------------------------");

    while (ush_browser_text_buf[i] != '\0') {
        char line[256];
        u64 j = 0ULL;

        while (ush_browser_text_buf[i] != '\0' && ush_browser_text_buf[i] != '\n' && j + 1ULL < (u64)sizeof(line)) {
            line[j++] = ush_browser_text_buf[i++];
        }
        line[j] = '\0';

        if (ush_browser_text_buf[i] == '\n') {
            i++;
        }

        if (line[0] != '\0') {
            ush_writeln(line);
            line_count++;
            if (line_count >= 220ULL) {
                ush_writeln("[browser] output truncated at 220 lines");
                break;
            }
        } else if (line_count > 0ULL) {
            ush_writeln("");
        }
    }

    if (ush_browser_link_count > 0ULL) {
        ush_writeln("");
        ush_writeln("[links]");
        for (k = 0ULL; k < ush_browser_link_count; k++) {
            (void)printf("  [%llu] " USH_BROWSER_ANSI_BLUE_UNDERLINE "%s" USH_BROWSER_ANSI_RESET " -> "
                         USH_BROWSER_ANSI_BLUE_UNDERLINE "%s" USH_BROWSER_ANSI_RESET "\n",
                         (unsigned long long)(k + 1ULL), ush_browser_links[k].text, ush_browser_links[k].href);
        }
    }
}

static int ush_browser_is_https_url(const char *text) {
    if (text == (const char *)0) {
        return 0;
    }

    return (text[0] == 'h' && text[1] == 't' && text[2] == 't' && text[3] == 'p' && text[4] == 's' &&
            text[5] == ':' && text[6] == '/' && text[7] == '/')
               ? 1
               : 0;
}

static int ush_browser_read_line(char *out_text, u64 out_size) {
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

    if (href_no_frag[0] == '/' && href_no_frag[1] == '/') {
        if (snprintf(out_source, (unsigned long)out_size, "http:%s", href_no_frag) <= 0) {
            return 0;
        }
        return 1;
    }

    if (href_no_frag[0] == '?') {
        char base_path_only[USH_BROWSER_PATH_MAX];

        if (ush_browser_copy_until_query_or_fragment(base.path, base_path_only, (u64)sizeof(base_path_only)) == 0) {
            return 0;
        }

        if (base.port == 80U) {
            if (snprintf(out_source, (unsigned long)out_size, "http://%s%s%s", base.host, base_path_only, href_no_frag) <=
                0) {
                return 0;
            }
        } else {
            if (snprintf(out_source, (unsigned long)out_size, "http://%s:%u%s%s", base.host, (unsigned int)base.port,
                         base_path_only, href_no_frag) <= 0) {
                return 0;
            }
        }
        return 1;
    }

    if (ush_browser_join_relative_path(base.path, href_no_frag, resolved_path, (u64)sizeof(resolved_path)) == 0) {
        return 0;
    }

    if (base.port == 80U) {
        if (snprintf(out_source, (unsigned long)out_size, "http://%s%s", base.host, resolved_path) <= 0) {
            return 0;
        }
    } else {
        if (snprintf(out_source, (unsigned long)out_size, "http://%s:%u%s", base.host, (unsigned int)base.port,
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

static int ush_browser_resolve_href(const char *base_source, const char *href, char *out_source, u64 out_size) {
    if (base_source == (const char *)0 || href == (const char *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (ush_browser_is_http_url(href) != 0 || ush_browser_is_https_url(href) != 0) {
        ush_copy(out_source, out_size, href);
        return (out_source[0] != '\0') ? 1 : 0;
    }

    if (ush_browser_is_http_url(base_source) != 0) {
        return ush_browser_resolve_http_href(base_source, href, out_source, out_size);
    }

    return ush_browser_resolve_local_href(base_source, href, out_source, out_size);
}

static int ush_browser_load_source(const ush_state *sh, const char *source, char *out_html, u64 out_html_cap, u64 *out_size) {
    if (sh == (const ush_state *)0 || source == (const char *)0 || out_html == (char *)0 || out_size == (u64 *)0) {
        return 0;
    }

    if (ush_browser_is_http_url(source) != 0) {
        if (cleonos_sys_net_available() == 0ULL) {
            return 0;
        }
        return ush_browser_fetch_http(source, out_html, out_html_cap, out_size);
    }

    if (ush_browser_is_https_url(source) != 0) {
        return 0;
    }

    return ush_browser_read_file(sh, source, out_html, out_html_cap, out_size);
}

static int ush_browser_push_history(char history[][USH_BROWSER_SOURCE_MAX], u64 *io_count, const char *source) {
    u64 i;

    if (history == (char(*)[USH_BROWSER_SOURCE_MAX])0 || io_count == (u64 *)0 || source == (const char *)0 ||
        source[0] == '\0') {
        return 0;
    }

    if (*io_count > 0ULL && ush_streq(history[*io_count - 1ULL], source) != 0) {
        return 1;
    }

    if (*io_count >= (u64)USH_BROWSER_HISTORY_MAX) {
        for (i = 1ULL; i < (u64)USH_BROWSER_HISTORY_MAX; i++) {
            ush_copy(history[i - 1ULL], (u64)USH_BROWSER_SOURCE_MAX, history[i]);
        }
        *io_count = (u64)USH_BROWSER_HISTORY_MAX - 1ULL;
    }

    ush_copy(history[*io_count], (u64)USH_BROWSER_SOURCE_MAX, source);
    (*io_count)++;
    return 1;
}

static int ush_browser_pop_history(char history[][USH_BROWSER_SOURCE_MAX], u64 *io_count, char *out_source, u64 out_size) {
    if (history == (char(*)[USH_BROWSER_SOURCE_MAX])0 || io_count == (u64 *)0 || out_source == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (*io_count == 0ULL) {
        return 0;
    }

    (*io_count)--;
    ush_copy(out_source, out_size, history[*io_count]);
    history[*io_count][0] = '\0';
    return 1;
}

static int ush_browser_parse_link_index(const char *text, u64 *out_index) {
    u64 i = 0ULL;
    u64 value = 0ULL;

    if (text == (const char *)0 || out_index == (u64 *)0 || text[0] == '\0') {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10ULL + (u64)(text[i] - '0');
        i++;
    }

    if (value == 0ULL) {
        return 0;
    }

    *out_index = value - 1ULL;
    return 1;
}

/* return: 0 fail, 1 ok, 2 help */
static int ush_browser_parse_args(const char *arg, char *out_source, u64 out_source_size) {
    char first[USH_PATH_MAX];
    const char *rest = "";

    if (out_source == (char *)0 || out_source_size == 0ULL) {
        return 0;
    }

    out_source[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    ush_copy(out_source, out_source_size, first);
    return (out_source[0] != '\0') ? 1 : 0;
}

static void ush_browser_usage(void) {
    ush_writeln("usage: browser <file.html|http://...>");
    ush_writeln("note: parser is gumbo from litehtml (no handwritten parser)");
}

static int ush_cmd_browser(const ush_state *sh, const char *arg) {
    char source[USH_BROWSER_SOURCE_MAX];
    char current_source[USH_BROWSER_SOURCE_MAX];
    char next_source[USH_BROWSER_SOURCE_MAX];
    char input_line[USH_BROWSER_INPUT_MAX];
    char history[USH_BROWSER_HISTORY_MAX][USH_BROWSER_SOURCE_MAX];
    u64 history_count = 0ULL;
    int parse_ret;
    u64 html_size = 0ULL;
    int loaded_once = 0;
    u64 i;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    for (i = 0ULL; i < (u64)USH_BROWSER_HISTORY_MAX; i++) {
        history[i][0] = '\0';
    }

    parse_ret = ush_browser_parse_args(arg, source, (u64)sizeof(source));
    if (parse_ret == 2) {
        ush_browser_usage();
        return 1;
    }

    if (parse_ret == 0) {
        ush_browser_usage();
        return 0;
    }

    if (ush_browser_is_http_url(source) != 0 || ush_browser_is_https_url(source) != 0) {
        ush_copy(current_source, (u64)sizeof(current_source), source);
    } else {
        if (ush_resolve_path(sh, source, current_source, (u64)sizeof(current_source)) == 0) {
            ush_copy(current_source, (u64)sizeof(current_source), source);
        }
    }

    for (;;) {
        if (ush_browser_load_source(sh, current_source, ush_browser_html_buf, (u64)sizeof(ush_browser_html_buf), &html_size) == 0) {
            if (ush_browser_is_https_url(current_source) != 0) {
                ush_writeln("browser: https:// is not supported yet");
            } else if (ush_browser_is_http_url(current_source) != 0) {
                ush_writeln("browser: http fetch failed");
            } else {
                ush_writeln("browser: file read failed");
            }

            if (loaded_once == 0 || ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                return 0;
            }
            ush_writeln("browser: returned to previous page");
            continue;
        }

        if (ush_browser_render_html(ush_browser_html_buf, html_size) == 0) {
            ush_writeln("browser: parse/render failed");
            if (loaded_once == 0 || ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                return 0;
            }
            ush_writeln("browser: returned to previous page");
            continue;
        }

        loaded_once = 1;
        ush_browser_print_rendered(current_source);
        ush_writeln("");
        ush_writeln("[browser] interactive mode");
        ush_writeln("[browser] <number>: open link   o <src>: open source   b: back   r: reload   q: quit");
        (void)fputs("browser> ", 1);

        if (ush_browser_read_line(input_line, (u64)sizeof(input_line)) == 0) {
            return 1;
        }
        ush_trim_line(input_line);

        if (input_line[0] == '\0') {
            continue;
        }

        if (ush_streq(input_line, "q") != 0 || ush_streq(input_line, "quit") != 0 || ush_streq(input_line, "exit") != 0) {
            return 1;
        }

        if (ush_streq(input_line, "r") != 0 || ush_streq(input_line, "reload") != 0) {
            continue;
        }

        if (ush_streq(input_line, "b") != 0 || ush_streq(input_line, "back") != 0) {
            if (ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                ush_writeln("browser: no history");
            }
            continue;
        }

        if (ush_streq(input_line, "h") != 0 || ush_streq(input_line, "help") != 0) {
            ush_writeln("[browser] commands:");
            ush_writeln("  <number>      open link by index");
            ush_writeln("  o <src>       open new URL/path");
            ush_writeln("  b             back");
            ush_writeln("  r             reload");
            ush_writeln("  q             quit");
            continue;
        }

        ush_zero(next_source, (u64)sizeof(next_source));
        if ((input_line[0] == 'o' && input_line[1] == ' ') || (input_line[0] == 'o' && input_line[1] == 'p' &&
                                                                input_line[2] == 'e' && input_line[3] == 'n' &&
                                                                input_line[4] == ' ')) {
            const char *payload = (input_line[1] == ' ') ? (input_line + 2) : (input_line + 5);
            while (*payload == ' ') {
                payload++;
            }
            ush_copy(next_source, (u64)sizeof(next_source), payload);
        } else {
            u64 link_index = 0ULL;
            if (ush_browser_parse_link_index(input_line, &link_index) != 0) {
                if (link_index >= ush_browser_link_count) {
                    ush_writeln("browser: link index out of range");
                    continue;
                }

                if (ush_browser_resolve_href(current_source, ush_browser_links[link_index].href, next_source,
                                             (u64)sizeof(next_source)) == 0) {
                    ush_writeln("browser: cannot resolve link target");
                    continue;
                }
            } else {
                ush_copy(next_source, (u64)sizeof(next_source), input_line);
            }
        }

        if (next_source[0] == '\0') {
            ush_writeln("browser: empty target");
            continue;
        }

        if (ush_browser_is_http_url(next_source) == 0 && ush_browser_is_https_url(next_source) == 0 && next_source[0] != '/') {
            char resolved_target[USH_BROWSER_SOURCE_MAX];

            if (ush_browser_resolve_href(current_source, next_source, resolved_target, (u64)sizeof(resolved_target)) != 0) {
                ush_copy(next_source, (u64)sizeof(next_source), resolved_target);
            } else if (ush_resolve_path(sh, next_source, resolved_target, (u64)sizeof(resolved_target)) != 0) {
                ush_copy(next_source, (u64)sizeof(next_source), resolved_target);
            }
        }

        if (ush_streq(next_source, current_source) != 0) {
            continue;
        }

        (void)ush_browser_push_history(history, &history_count, current_source);
        ush_copy(current_source, (u64)sizeof(current_source), next_source);
    }

}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    char argv_buf[USH_BROWSER_SOURCE_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";
    u64 argc = 0ULL;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_zero(argv_buf, (u64)sizeof(argv_buf));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "browser") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
            }
        }
    }

    if (has_context == 0) {
        argc = cleonos_sys_proc_argc();
        if (argc > 1ULL) {
            (void)cleonos_sys_proc_argv(1ULL, argv_buf, (u64)sizeof(argv_buf));
            arg = argv_buf;
        }
    }

    success = ush_cmd_browser(&sh, arg);

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
