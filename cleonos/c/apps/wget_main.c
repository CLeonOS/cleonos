#include "cmd_runtime.h"
#include "tls/cleonos_tls.h"
#include <stdio.h>
#include <stdlib.h>

#define USH_WGET_HOST_MAX 128U
#define USH_WGET_PATH_MAX 256U
#define USH_WGET_DNS_PACKET_MAX 512U
#define USH_WGET_RECV_CHUNK 2048U
#define USH_WGET_HEADER_MAX 4096U
#define USH_WGET_TIMEOUT_SECONDS 10ULL
#define USH_WGET_TCP_POLL_BUDGET 200000000ULL
#define USH_WGET_RECV_POLL_BUDGET 60000ULL

typedef unsigned char u8;
typedef unsigned short u16;

typedef struct ush_wget_chunk_state {
    u64 remaining;
    u64 size_acc;
    u64 trailer_line_len;
    int state;
    int has_digit;
    int in_extension;
    int done;
} ush_wget_chunk_state;

#define USH_WGET_CHUNK_SIZE 0
#define USH_WGET_CHUNK_SIZE_LF 1
#define USH_WGET_CHUNK_DATA 2
#define USH_WGET_CHUNK_DATA_LF 3
#define USH_WGET_CHUNK_TRAILER 4
#define USH_WGET_CHUNK_TRAILER_LF 5

static u64 ush_wget_timeout_ticks(void) {
    u64 hz = cleonos_sys_timer_hz();
    if (hz == 0ULL || hz == (u64)-1) {
        hz = 100ULL;
    }
    return hz * USH_WGET_TIMEOUT_SECONDS;
}

static u64 ush_wget_deadline_after_timeout(void) {
    return cleonos_sys_timer_ticks() + ush_wget_timeout_ticks();
}

static int ush_wget_deadline_expired(u64 deadline_tick) {
    return (cleonos_sys_timer_ticks() >= deadline_tick) ? 1 : 0;
}

static const char *ush_wget_tcp_error_text(u64 error_code) {
    switch (error_code) {
    case 0ULL:
        return "none";
    case 1ULL:
        return "network unavailable";
    case 2ULL:
        return "bad address or port";
    case 3ULL:
        return "arp/gateway resolve failed";
    case 4ULL:
        return "syn transmit failed";
    case 5ULL:
        return "connection reset/refused";
    case 6ULL:
        return "syn-ack timeout";
    case 7ULL:
        return "stale ack from previous connection";
    default:
        return "unknown";
    }
}

static void ush_wget_print_tcp_connect_failed(void) {
    u64 err = cleonos_sys_net_tcp_last_error();
    (void)printf((ush_locale_is_zh() != 0) ? "wget: TCP 连接失败: %s (%llu)\n"
                                           : "wget: tcp connect failed: %s (%llu)\n",
                 ush_wget_tcp_error_text(err), (unsigned long long)err);
}

typedef struct ush_wget_url {
    char host[USH_WGET_HOST_MAX];
    char path[USH_WGET_PATH_MAX];
    u16 port;
    int tls;
} ush_wget_url;

typedef struct ush_wget_args {
    char url[USH_ARG_MAX];
    char output[USH_PATH_MAX];
} ush_wget_args;

static u16 ush_wget_read_be16(const u8 *ptr) {
    return (u16)(((u16)ptr[0] << 8U) | (u16)ptr[1]);
}

static void ush_wget_write_be16(u8 *ptr, u16 value) {
    ptr[0] = (u8)((value >> 8U) & 0xFFU);
    ptr[1] = (u8)(value & 0xFFU);
}

static int ush_wget_parse_ipv4_be(const char *text, u64 *out_ipv4_be) {
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

    *out_ipv4_be = ((acc << 8ULL) | value) & 0xFFFFFFFFULL;
    return 1;
}

static void ush_wget_print_ipv4(u64 ipv4_be) {
    (void)printf("%u.%u.%u.%u", (unsigned int)((ipv4_be >> 24ULL) & 0xFFULL),
                 (unsigned int)((ipv4_be >> 16ULL) & 0xFFULL), (unsigned int)((ipv4_be >> 8ULL) & 0xFFULL),
                 (unsigned int)(ipv4_be & 0xFFULL));
}

static int ush_wget_parse_url(const char *url, ush_wget_url *out_url) {
    const char *p;
    const char *host_begin;
    const char *host_end;
    const char *path_begin;
    u64 host_len;
    u64 path_len;
    u64 i;
    u64 scheme_len;

    if (url == (const char *)0 || out_url == (ush_wget_url *)0) {
        return 0;
    }

    ush_zero(out_url, (u64)sizeof(*out_url));
    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' && url[4] == ':' && url[5] == '/' &&
        url[6] == '/') {
        out_url->tls = 0;
        out_url->port = 80U;
        scheme_len = 7ULL;
    } else if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' && url[4] == 's' && url[5] == ':' &&
               url[6] == '/' && url[7] == '/') {
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

static int ush_wget_dns_encode_qname(const char *host, u8 *out, u64 out_cap, u64 *out_len) {
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
        if (label_len == 0ULL || label_len > 63ULL || at + 1ULL + label_len + 1ULL > out_cap) {
            return 0;
        }
        out[at++] = (u8)label_len;
        for (i = 0ULL; i < label_len; i++) {
            out[at++] = (u8)p[i];
        }
        p = (*dot == '.') ? dot + 1 : dot;
    }

    out[at++] = 0U;
    *out_len = at;
    return 1;
}

static int ush_wget_dns_skip_name(const u8 *msg, u64 msg_len, u64 *io_off) {
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
        if ((len & 0xC0U) != 0U || pos + 1ULL + (u64)len > msg_len) {
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

static int ush_wget_dns_parse_first_a(const u8 *resp, u64 resp_len, u16 expected_id, u64 *out_ipv4_be) {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 i;
    u64 off = 12ULL;

    if (resp == (const u8 *)0 || out_ipv4_be == (u64 *)0 || resp_len < 12ULL) {
        return 0;
    }

    id = ush_wget_read_be16(&resp[0]);
    flags = ush_wget_read_be16(&resp[2]);
    qdcount = ush_wget_read_be16(&resp[4]);
    ancount = ush_wget_read_be16(&resp[6]);
    if (id != expected_id || (flags & 0x8000U) == 0U || (flags & 0x000FU) != 0U) {
        return 0;
    }

    for (i = 0U; i < qdcount; i++) {
        if (ush_wget_dns_skip_name(resp, resp_len, &off) == 0 || off + 4ULL > resp_len) {
            return 0;
        }
        off += 4ULL;
    }

    for (i = 0U; i < ancount; i++) {
        u16 type;
        u16 klass;
        u16 rdlen;
        if (ush_wget_dns_skip_name(resp, resp_len, &off) == 0 || off + 10ULL > resp_len) {
            return 0;
        }
        type = ush_wget_read_be16(&resp[off + 0ULL]);
        klass = ush_wget_read_be16(&resp[off + 2ULL]);
        rdlen = ush_wget_read_be16(&resp[off + 8ULL]);
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

static int ush_wget_dns_resolve_ipv4(const char *host, u64 *out_ipv4_be) {
    u64 dns_server;
    u8 query[USH_WGET_DNS_PACKET_MAX];
    u8 answer[USH_WGET_DNS_PACKET_MAX];
    u64 qname_len = 0ULL;
    u64 query_len;
    u16 txid;
    u16 src_port;
    cleonos_net_udp_send_req send_req;
    u64 deadline_tick;

    if (host == (const char *)0 || out_ipv4_be == (u64 *)0 || host[0] == '\0') {
        return 0;
    }

    dns_server = cleonos_sys_net_dns_server();
    if (dns_server == 0ULL) {
        return 0;
    }

    txid = (u16)((cleonos_sys_timer_ticks() ^ 0x5747ULL) & 0xFFFFULL);
    src_port = (u16)(42000U + (txid % 20000U));
    (void)memset(query, 0, sizeof(query));
    if (ush_wget_dns_encode_qname(host, query + 12ULL, (u64)sizeof(query) - 12ULL, &qname_len) == 0) {
        return 0;
    }

    ush_wget_write_be16(&query[0], txid);
    ush_wget_write_be16(&query[2], 0x0100U);
    ush_wget_write_be16(&query[4], 1U);
    query_len = 12ULL + qname_len;
    if (query_len + 4ULL > (u64)sizeof(query)) {
        return 0;
    }
    ush_wget_write_be16(&query[query_len], 1U);
    ush_wget_write_be16(&query[query_len + 2ULL], 1U);
    query_len += 4ULL;

    send_req.dst_ipv4_be = dns_server;
    send_req.dst_port = 53ULL;
    send_req.src_port = (u64)src_port;
    send_req.payload_ptr = (u64)(usize)query;
    send_req.payload_len = query_len;
    if (cleonos_sys_net_udp_send(&send_req) != query_len) {
        return 0;
    }

    deadline_tick = ush_wget_deadline_after_timeout();
    while (ush_wget_deadline_expired(deadline_tick) == 0) {
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
            (void)cleonos_sys_sleep_ms(20ULL);
            continue;
        }
        if (src_p == 53ULL && src_ip == dns_server && ush_wget_dns_parse_first_a(answer, got, txid, out_ipv4_be) != 0) {
            return 1;
        }
    }

    return 0;
}

static int ush_wget_write_all(u64 fd, const void *buffer, u64 size) {
    const u8 *bytes = (const u8 *)buffer;
    u64 done = 0ULL;

    while (done < size) {
        u64 written = cleonos_sys_fd_write(fd, bytes + done, size - done);
        if (written == 0ULL || written == (u64)-1) {
            return 0;
        }
        done += written;
    }
    return 1;
}

static char ush_wget_ascii_lower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static int ush_wget_header_name_match_icase(const u8 *line, u64 line_len, const char *name) {
    u64 i = 0ULL;

    if (line == (const u8 *)0 || name == (const char *)0) {
        return 0;
    }

    while (name[i] != '\0') {
        if (i >= line_len || ush_wget_ascii_lower((char)line[i]) != ush_wget_ascii_lower(name[i])) {
            return 0;
        }
        i++;
    }

    return (i < line_len && line[i] == (u8)':') ? 1 : 0;
}

static int ush_wget_line_has_token_icase(const u8 *value, u64 value_len, const char *token) {
    u64 token_len;
    u64 i;

    if (value == (const u8 *)0 || token == (const char *)0) {
        return 0;
    }

    token_len = ush_strlen(token);
    if (token_len == 0ULL || value_len < token_len) {
        return 0;
    }

    for (i = 0ULL; i + token_len <= value_len; i++) {
        u64 j;
        int match = 1;
        if (i > 0ULL && value[i - 1ULL] != (u8)',' && value[i - 1ULL] != (u8)' ' && value[i - 1ULL] != (u8)'\t') {
            continue;
        }
        for (j = 0ULL; j < token_len; j++) {
            if (ush_wget_ascii_lower((char)value[i + j]) != ush_wget_ascii_lower(token[j])) {
                match = 0;
                break;
            }
        }
        if (match != 0 &&
            (i + token_len == value_len || value[i + token_len] == (u8)',' || value[i + token_len] == (u8)' ' ||
             value[i + token_len] == (u8)'\t' || value[i + token_len] == (u8)';')) {
            return 1;
        }
    }

    return 0;
}

static int ush_wget_parse_content_length(const u8 *value, u64 value_len, u64 *out_len) {
    u64 i = 0ULL;
    u64 acc = 0ULL;
    int has_digit = 0;

    if (value == (const u8 *)0 || out_len == (u64 *)0) {
        return 0;
    }

    while (i < value_len && (value[i] == (u8)' ' || value[i] == (u8)'\t')) {
        i++;
    }
    while (i < value_len && value[i] >= (u8)'0' && value[i] <= (u8)'9') {
        u64 next = acc * 10ULL + (u64)(value[i] - (u8)'0');
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

static int ush_wget_parse_http_headers(const u8 *header, u64 body_offset, int *out_chunked, u64 *out_content_len,
                                       int *out_has_content_len) {
    u64 off = 0ULL;

    if (header == (const u8 *)0 || out_chunked == (int *)0 || out_content_len == (u64 *)0 ||
        out_has_content_len == (int *)0) {
        return 0;
    }

    *out_chunked = 0;
    *out_content_len = 0ULL;
    *out_has_content_len = 0;

    while (off < body_offset) {
        u64 line_start = off;
        u64 line_len;

        while (off < body_offset && header[off] != (u8)'\r' && header[off] != (u8)'\n') {
            off++;
        }
        line_len = off - line_start;
        if (off < body_offset && header[off] == (u8)'\r') {
            off++;
        }
        if (off < body_offset && header[off] == (u8)'\n') {
            off++;
        }

        if (line_len == 0ULL) {
            break;
        }

        if (ush_wget_header_name_match_icase(header + line_start, line_len, "Transfer-Encoding") != 0) {
            u64 key_len = (u64)(sizeof("Transfer-Encoding") - 1U + 1U);
            const u8 *value = header + line_start + key_len;
            u64 value_len = line_len - key_len;
            while (value_len > 0ULL && (*value == (u8)' ' || *value == (u8)'\t')) {
                value++;
                value_len--;
            }
            if (ush_wget_line_has_token_icase(value, value_len, "chunked") != 0) {
                *out_chunked = 1;
            }
        } else if (ush_wget_header_name_match_icase(header + line_start, line_len, "Content-Length") != 0) {
            u64 key_len = (u64)(sizeof("Content-Length") - 1U + 1U);
            const u8 *value = header + line_start + key_len;
            u64 value_len = line_len - key_len;
            u64 parsed = 0ULL;
            if (ush_wget_parse_content_length(value, value_len, &parsed) != 0) {
                *out_content_len = parsed;
                *out_has_content_len = 1;
            }
        }
    }

    return 1;
}

static int ush_wget_chunk_finish_size_line(ush_wget_chunk_state *state) {
    if (state == (ush_wget_chunk_state *)0 || state->has_digit == 0) {
        return 0;
    }

    state->remaining = state->size_acc;
    state->size_acc = 0ULL;
    state->has_digit = 0;
    state->in_extension = 0;
    if (state->remaining == 0ULL) {
        state->trailer_line_len = 0ULL;
        state->state = USH_WGET_CHUNK_TRAILER;
    } else {
        state->state = USH_WGET_CHUNK_DATA;
    }
    return 1;
}

static int ush_wget_chunked_write_feed(ush_wget_chunk_state *state, u64 fd, const u8 *data, u64 len, u64 *saved) {
    u64 i = 0ULL;

    if (state == (ush_wget_chunk_state *)0 || data == (const u8 *)0 || saved == (u64 *)0) {
        return 0;
    }

    while (i < len && state->done == 0) {
        u8 ch = data[i];

        if (state->state == USH_WGET_CHUNK_DATA) {
            u64 n = state->remaining;
            if (n > len - i) {
                n = len - i;
            }
            if (n == 0ULL) {
                state->state = USH_WGET_CHUNK_DATA_LF;
                continue;
            }
            if (ush_wget_write_all(fd, data + i, n) == 0) {
                return 0;
            }
            *saved += n;
            state->remaining -= n;
            i += n;
            if (state->remaining == 0ULL) {
                state->state = USH_WGET_CHUNK_DATA_LF;
            }
            continue;
        }

        if (state->state == USH_WGET_CHUNK_SIZE) {
            u64 v = 0ULL;

            if (ch == (u8)'\r') {
                state->state = USH_WGET_CHUNK_SIZE_LF;
                i++;
                continue;
            }
            if (ch == (u8)'\n') {
                if (ush_wget_chunk_finish_size_line(state) == 0) {
                    return 0;
                }
                i++;
                continue;
            }
            if (ch == (u8)';') {
                if (state->has_digit == 0) {
                    return 0;
                }
                state->in_extension = 1;
                i++;
                continue;
            }
            if (state->in_extension != 0 || ch == (u8)' ' || ch == (u8)'\t') {
                i++;
                continue;
            }
            if (ch >= (u8)'0' && ch <= (u8)'9') {
                v = (u64)(ch - (u8)'0');
            } else if (ch >= (u8)'a' && ch <= (u8)'f') {
                v = (u64)(ch - (u8)'a') + 10ULL;
            } else if (ch >= (u8)'A' && ch <= (u8)'F') {
                v = (u64)(ch - (u8)'A') + 10ULL;
            } else {
                return 0;
            }
            if (state->size_acc > (0xFFFFFFFFFFFFFFFFULL >> 4U)) {
                return 0;
            }
            state->size_acc = (state->size_acc << 4U) | v;
            state->has_digit = 1;
            i++;
            continue;
        }

        if (state->state == USH_WGET_CHUNK_SIZE_LF) {
            if (ch != (u8)'\n' || ush_wget_chunk_finish_size_line(state) == 0) {
                return 0;
            }
            i++;
            continue;
        }

        if (state->state == USH_WGET_CHUNK_DATA_LF) {
            if (ch == (u8)'\r') {
                i++;
                continue;
            }
            if (ch != (u8)'\n') {
                return 0;
            }
            state->state = USH_WGET_CHUNK_SIZE;
            i++;
            continue;
        }

        if (state->state == USH_WGET_CHUNK_TRAILER) {
            if (ch == (u8)'\r') {
                state->state = USH_WGET_CHUNK_TRAILER_LF;
            } else if (ch == (u8)'\n') {
                if (state->trailer_line_len == 0ULL) {
                    state->done = 1;
                } else {
                    state->trailer_line_len = 0ULL;
                }
            } else {
                state->trailer_line_len++;
            }
            i++;
            continue;
        }

        if (state->state == USH_WGET_CHUNK_TRAILER_LF) {
            if (ch != (u8)'\n') {
                return 0;
            }
            if (state->trailer_line_len == 0ULL) {
                state->done = 1;
            } else {
                state->trailer_line_len = 0ULL;
                state->state = USH_WGET_CHUNK_TRAILER;
            }
            i++;
            continue;
        }

        return 0;
    }

    return 1;
}

static int ush_wget_find_header_end(const u8 *data, u64 len, u64 *out_body_offset) {
    u64 i;

    if (data == (const u8 *)0 || out_body_offset == (u64 *)0) {
        return 0;
    }

    for (i = 0ULL; i + 3ULL < len; i++) {
        if (data[i] == (u8)'\r' && data[i + 1ULL] == (u8)'\n' && data[i + 2ULL] == (u8)'\r' &&
            data[i + 3ULL] == (u8)'\n') {
            *out_body_offset = i + 4ULL;
            return 1;
        }
    }
    for (i = 0ULL; i + 1ULL < len; i++) {
        if (data[i] == (u8)'\n' && data[i + 1ULL] == (u8)'\n') {
            *out_body_offset = i + 2ULL;
            return 1;
        }
    }
    return 0;
}

static int ush_wget_status_code(const u8 *header, u64 len) {
    if (header == (const u8 *)0 || len < 12ULL) {
        return 0;
    }
    if (!(header[0] == (u8)'H' && header[1] == (u8)'T' && header[2] == (u8)'T' && header[3] == (u8)'P')) {
        return 0;
    }
    while (len > 0ULL && *header != (u8)' ') {
        header++;
        len--;
    }
    while (len > 0ULL && *header == (u8)' ') {
        header++;
        len--;
    }
    if (len < 3ULL || header[0] < (u8)'0' || header[0] > (u8)'9' || header[1] < (u8)'0' || header[1] > (u8)'9' ||
        header[2] < (u8)'0' || header[2] > (u8)'9') {
        return 0;
    }
    return ((int)(header[0] - (u8)'0') * 100) + ((int)(header[1] - (u8)'0') * 10) + (int)(header[2] - (u8)'0');
}

static void ush_wget_default_output_name(const char *path, char *out, u64 out_size) {
    const char *base = path;
    u64 i;
    u64 len = 0ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    out[0] = '\0';
    if (path == (const char *)0 || path[0] == '\0') {
        ush_copy(out, out_size, "index.html");
        return;
    }
    for (i = 0ULL; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            base = path + i + 1ULL;
        }
    }
    if (base[0] == '\0' || base[0] == '?' || base[0] == '#') {
        ush_copy(out, out_size, "index.html");
        return;
    }
    while (base[len] != '\0' && base[len] != '?' && base[len] != '#' && len + 1ULL < out_size) {
        out[len] = base[len];
        len++;
    }
    if (len == 0ULL) {
        ush_copy(out, out_size, "index.html");
        return;
    }
    out[len] = '\0';
}

static int ush_wget_parse_args(const char *arg, ush_wget_args *out_args) {
    char first[USH_ARG_MAX];
    char second[USH_ARG_MAX];
    char third[USH_ARG_MAX];
    const char *rest = "";
    const char *rest2 = "";
    const char *rest3 = "";

    if (arg == (const char *)0 || out_args == (ush_wget_args *)0) {
        return 0;
    }
    ush_zero(out_args, (u64)sizeof(*out_args));
    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }
    if (ush_streq(first, "-O") != 0) {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0 ||
            ush_split_first_and_rest(rest2, third, (u64)sizeof(third), &rest3) == 0 || rest3[0] != '\0') {
            return 0;
        }
        ush_copy(out_args->output, (u64)sizeof(out_args->output), second);
        ush_copy(out_args->url, (u64)sizeof(out_args->url), third);
        return 1;
    }

    ush_copy(out_args->url, (u64)sizeof(out_args->url), first);
    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0 || rest2[0] != '\0') {
            return 0;
        }
        ush_copy(out_args->output, (u64)sizeof(out_args->output), second);
    }
    return 1;
}

static int ush_cmd_wget(const ush_state *sh, const char *arg) {
    ush_wget_args args;
    ush_wget_url url;
    u64 dst_ipv4_be = 0ULL;
    u64 out_fd = (u64)-1;
    char output_arg[USH_PATH_MAX];
    char output_abs[USH_PATH_MAX];
    char request[1024];
    int request_len;
    cleonos_net_tcp_connect_req conn_req;
    cleonos_net_tcp_send_req send_req;
    cleonos_tls_conn *tls_conn = (cleonos_tls_conn *)0;
    int tcp_open = 0;
    int tls_open = 0;
    u8 header[USH_WGET_HEADER_MAX];
    u64 header_used = 0ULL;
    int header_done = 0;
    int status = 0;
    u64 saved = 0ULL;
    u64 idle_deadline_tick;
    int is_chunked = 0;
    int has_content_length = 0;
    u64 content_length = 0ULL;
    ush_wget_chunk_state chunk_state;
    int ok = 0;

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0) {
        ush_writeln_i18n("usage: wget <http://host[:port]/path> [output]",
                         "用法: wget <http://host[:port]/path> [output]");
        ush_writeln("       wget <https://host[:port]/path> [output]");
        ush_writeln("       wget -O <output> <url>");
        ush_writeln_i18n("note: default network timeout is 10 seconds", "提示: 默认网络超时为 10 秒");
        ush_writeln_i18n("note: TLS uses encryption + SNI; certificate verification is not available yet",
                         "提示: TLS 使用加密和 SNI；证书验证暂不可用");
        return 0;
    }

    if (sh == (const ush_state *)0 || ush_wget_parse_args(arg, &args) == 0 || ush_wget_parse_url(args.url, &url) == 0) {
        ush_writeln_i18n("wget: invalid arguments or URL", "wget: 参数或 URL 无效");
        return 0;
    }

    if (args.output[0] == '\0') {
        ush_wget_default_output_name(url.path, output_arg, (u64)sizeof(output_arg));
    } else {
        ush_copy(output_arg, (u64)sizeof(output_arg), args.output);
    }

    if (ush_resolve_path(sh, output_arg, output_abs, (u64)sizeof(output_abs)) == 0) {
        ush_writeln_i18n("wget: invalid output path", "wget: 输出路径无效");
        return 0;
    }

    if (cleonos_sys_net_available() == 0ULL) {
        ush_writeln_i18n("wget: network unavailable", "wget: 网络不可用");
        return 0;
    }

    if (ush_wget_parse_ipv4_be(url.host, &dst_ipv4_be) == 0) {
        if (ush_wget_dns_resolve_ipv4(url.host, &dst_ipv4_be) == 0) {
            (void)printf((ush_locale_is_zh() != 0) ? "wget: DNS 解析失败 for %s\n"
                                                    : "wget: DNS resolve failed for %s\n",
                         url.host);
            return 0;
        }
    }

    out_fd = cleonos_sys_fd_open(output_abs, CLEONOS_O_WRONLY | CLEONOS_O_CREAT | CLEONOS_O_TRUNC, 0ULL);
    if (out_fd == (u64)-1) {
        ush_writeln_i18n("wget: output open failed", "wget: 打开输出文件失败");
        return 0;
    }

    ush_write_i18n_label("wget: connect", "wget: 连接");
    ush_write(" ");
    ush_wget_print_ipv4(dst_ipv4_be);
    (void)printf(":%u%s\n", (unsigned int)url.port, (url.tls != 0) ? " tls" : "");

    if (url.tls != 0) {
        tls_conn = (cleonos_tls_conn *)malloc(sizeof(*tls_conn));
        if (tls_conn == (cleonos_tls_conn *)0) {
            ush_writeln_i18n("wget: tls allocation failed", "wget: TLS 分配失败");
            goto done;
        }
        ush_zero(tls_conn, (u64)sizeof(*tls_conn));
        if (cleonos_tls_connect_deadline(tls_conn, dst_ipv4_be, url.port, url.host, USH_WGET_TCP_POLL_BUDGET,
                                         ush_wget_deadline_after_timeout()) == 0) {
            char tls_error[96];
            cleonos_tls_error_text(cleonos_tls_last_error(tls_conn), tls_error, (u64)sizeof(tls_error));
            (void)printf((ush_locale_is_zh() != 0) ? "wget: TLS 连接失败: %s\n"
                                                    : "wget: tls connect failed: %s\n",
                         tls_error);
            goto done;
        }
        tls_open = 1;
    } else {
        ush_zero(&conn_req, (u64)sizeof(conn_req));
        conn_req.dst_ipv4_be = dst_ipv4_be;
        conn_req.dst_port = (u64)url.port;
        conn_req.src_port = 0ULL;
        conn_req.poll_budget = USH_WGET_TCP_POLL_BUDGET;
        if (cleonos_sys_net_tcp_connect(&conn_req) == 0ULL) {
            ush_wget_print_tcp_connect_failed();
            goto done;
        }
        tcp_open = 1;
    }

    if ((url.tls == 0 && url.port == 80U) || (url.tls != 0 && url.port == 443U)) {
        request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: cleonos-wget/1.0\r\nAccept: */*\r\n"
                               "Accept-Encoding: identity\r\n"
                               "Connection: close\r\n\r\n",
                               url.path, url.host);
    } else {
        request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: cleonos-wget/1.0\r\nAccept: */*\r\n"
                               "Accept-Encoding: identity\r\n"
                               "Connection: close\r\n\r\n",
                               url.path, url.host, (unsigned int)url.port);
    }
    if (request_len <= 0 || (u64)request_len >= (u64)sizeof(request)) {
        ush_writeln_i18n("wget: request build failed", "wget: 请求构建失败");
        goto done;
    }

    if (url.tls != 0) {
        if (cleonos_tls_write_all(tls_conn, request, (u64)request_len) == 0) {
            char tls_error[96];
            cleonos_tls_error_text(cleonos_tls_last_error(tls_conn), tls_error, (u64)sizeof(tls_error));
            (void)printf((ush_locale_is_zh() != 0) ? "wget: TLS 发送失败: %s\n"
                                                    : "wget: tls send failed: %s\n",
                         tls_error);
            goto done;
        }
    } else {
        send_req.payload_ptr = (u64)(usize)request;
        send_req.payload_len = (u64)request_len;
        send_req.poll_budget = USH_WGET_TCP_POLL_BUDGET;
        if (cleonos_sys_net_tcp_send(&send_req) != (u64)request_len) {
            ush_writeln_i18n("wget: tcp send failed", "wget: TCP 发送失败");
            goto done;
        }
    }

    ush_zero(&chunk_state, (u64)sizeof(chunk_state));
    idle_deadline_tick = ush_wget_deadline_after_timeout();
    for (;;) {
        u8 chunk[USH_WGET_RECV_CHUNK];
        u64 got = 0ULL;

        if (url.tls != 0) {
            int tls_got = cleonos_tls_read(tls_conn, chunk, (u64)sizeof(chunk));
            if (tls_got < 0) {
                char tls_error[96];
                cleonos_tls_error_text(cleonos_tls_last_error(tls_conn), tls_error, (u64)sizeof(tls_error));
                (void)printf((ush_locale_is_zh() != 0) ? "wget: TLS 接收失败: %s\n"
                                                        : "wget: tls recv failed: %s\n",
                             tls_error);
                goto done;
            }
            got = (u64)tls_got;
        } else {
            cleonos_net_tcp_recv_req recv_req;
            recv_req.out_payload_ptr = (u64)(usize)chunk;
            recv_req.payload_capacity = (u64)sizeof(chunk);
            recv_req.poll_budget = USH_WGET_RECV_POLL_BUDGET;
            got = cleonos_sys_net_tcp_recv(&recv_req);
        }
        if (got == 0ULL) {
            if (url.tls != 0 && cleonos_tls_eof(tls_conn) != 0) {
                break;
            }
            if (header_done != 0) {
                if (is_chunked == 0 && has_content_length != 0 && saved >= content_length) {
                    break;
                }
                if (is_chunked != 0 && chunk_state.done != 0) {
                    break;
                }
            }
            if (ush_wget_deadline_expired(idle_deadline_tick) != 0) {
                if (header_done != 0 && saved > 0ULL && has_content_length == 0 && is_chunked == 0) {
                    break;
                }
                ush_writeln_i18n("wget: network timeout", "wget: 网络超时");
                goto done;
            }
            (void)cleonos_sys_sleep_ms(10ULL);
            continue;
        }

        idle_deadline_tick = ush_wget_deadline_after_timeout();
        if (header_done == 0) {
            u64 body_offset = 0ULL;
            if (header_used + got > (u64)sizeof(header)) {
                ush_writeln_i18n("wget: HTTP header too large", "wget: HTTP 头过大");
                goto done;
            }
            memcpy(header + header_used, chunk, (usize)got);
            header_used += got;
            if (ush_wget_find_header_end(header, header_used, &body_offset) == 0) {
                continue;
            }
            header_done = 1;
            status = ush_wget_status_code(header, header_used);
            if (status >= 400) {
                (void)printf((ush_locale_is_zh() != 0) ? "wget: HTTP 状态 %d\n"
                                                        : "wget: HTTP status %d\n",
                             status);
                goto done;
            }
            if (ush_wget_parse_http_headers(header, body_offset, &is_chunked, &content_length, &has_content_length) ==
                0) {
                ush_writeln_i18n("wget: invalid HTTP headers", "wget: HTTP 头无效");
                goto done;
            }
            if (header_used > body_offset) {
                u64 body_len = header_used - body_offset;
                if (is_chunked != 0) {
                    if (ush_wget_chunked_write_feed(&chunk_state, out_fd, header + body_offset, body_len, &saved) ==
                        0) {
                        ush_writeln_i18n("wget: invalid chunked HTTP body", "wget: chunked HTTP body 无效");
                        goto done;
                    }
                    if (chunk_state.done != 0) {
                        break;
                    }
                } else if (has_content_length != 0) {
                    if (body_len > content_length) {
                        body_len = content_length;
                    }
                    if (body_len > 0ULL && ush_wget_write_all(out_fd, header + body_offset, body_len) == 0) {
                        ush_writeln_i18n("wget: write failed", "wget: 写入失败");
                        goto done;
                    }
                    saved += body_len;
                    if (saved >= content_length) {
                        break;
                    }
                } else if (ush_wget_write_all(out_fd, header + body_offset, body_len) == 0) {
                    ush_writeln_i18n("wget: write failed", "wget: 写入失败");
                    goto done;
                } else {
                    saved += body_len;
                }
            }
            continue;
        }

        if (is_chunked != 0) {
            if (ush_wget_chunked_write_feed(&chunk_state, out_fd, chunk, got, &saved) == 0) {
                ush_writeln_i18n("wget: invalid chunked HTTP body", "wget: chunked HTTP body 无效");
                goto done;
            }
            if (chunk_state.done != 0) {
                break;
            }
            continue;
        }

        if (has_content_length != 0) {
            if (got > content_length - saved) {
                got = content_length - saved;
            }
            if (got == 0ULL || saved + got >= content_length) {
                if (got > 0ULL && ush_wget_write_all(out_fd, chunk, got) == 0) {
                    ush_writeln_i18n("wget: write failed", "wget: 写入失败");
                    goto done;
                }
                saved += got;
                break;
            }
        }

        if (ush_wget_write_all(out_fd, chunk, got) == 0) {
            ush_writeln_i18n("wget: write failed", "wget: 写入失败");
            goto done;
        }
        saved += got;
    }

    if (header_done == 0) {
        ush_writeln_i18n("wget: no HTTP response", "wget: 没有 HTTP 响应");
        goto done;
    }

    (void)printf((ush_locale_is_zh() != 0) ? "wget: 已保存 %llu bytes to %s\n"
                                           : "wget: saved %llu bytes to %s\n",
                 (unsigned long long)saved, output_abs);
    ok = 1;

done:
    if (tls_open != 0) {
        cleonos_tls_close(tls_conn, USH_WGET_TCP_POLL_BUDGET);
    } else if (tcp_open != 0) {
        (void)cleonos_sys_net_tcp_close(USH_WGET_TCP_POLL_BUDGET);
    }
    if (tls_conn != (cleonos_tls_conn *)0) {
        free(tls_conn);
    }
    if (out_fd != (u64)-1) {
        (void)cleonos_sys_fd_close(out_fd);
    }
    return ok;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "wget") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_wget(&sh, arg);

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
