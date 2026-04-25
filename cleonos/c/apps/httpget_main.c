#include "cmd_runtime.h"
#include <stdio.h>

#define USH_HTTPGET_HOST_MAX 128U
#define USH_HTTPGET_PATH_MAX 256U
#define USH_HTTPGET_DNS_PACKET_MAX 512U
#define USH_HTTPGET_RECV_CHUNK 2048U
#define USH_HTTPGET_TCP_POLL_BUDGET 200000000ULL
#define USH_HTTPGET_TCP_RECV_IDLE_LOOPS 120ULL

typedef unsigned char u8;
typedef unsigned short u16;

typedef struct ush_httpget_url {
    char host[USH_HTTPGET_HOST_MAX];
    char path[USH_HTTPGET_PATH_MAX];
    u16 port;
} ush_httpget_url;

static u16 ush_httpget_read_be16(const u8 *ptr) {
    return (u16)(((u16)ptr[0] << 8U) | (u16)ptr[1]);
}

static void ush_httpget_write_be16(u8 *ptr, u16 value) {
    ptr[0] = (u8)((value >> 8U) & 0xFFU);
    ptr[1] = (u8)(value & 0xFFU);
}

static int ush_httpget_parse_ipv4_be(const char *text, u64 *out_ipv4_be) {
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

static void ush_httpget_print_ipv4(u64 ipv4_be) {
    unsigned int a = (unsigned int)((ipv4_be >> 24ULL) & 0xFFULL);
    unsigned int b = (unsigned int)((ipv4_be >> 16ULL) & 0xFFULL);
    unsigned int c = (unsigned int)((ipv4_be >> 8ULL) & 0xFFULL);
    unsigned int d = (unsigned int)(ipv4_be & 0xFFULL);

    (void)printf("%u.%u.%u.%u", a, b, c, d);
}

static int ush_httpget_parse_url(const char *url, ush_httpget_url *out_url) {
    const char *p;
    const char *host_begin;
    const char *host_end;
    const char *path_begin;
    u64 host_len;
    u64 path_len;
    u64 i;

    if (url == (const char *)0 || out_url == (ush_httpget_url *)0) {
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

static int ush_httpget_dns_encode_qname(const char *host, u8 *out, u64 out_cap, u64 *out_len) {
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

static int ush_httpget_dns_skip_name(const u8 *msg, u64 msg_len, u64 *io_off) {
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

static int ush_httpget_dns_parse_first_a(const u8 *resp, u64 resp_len, u16 expected_id, u64 *out_ipv4_be) {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 i;
    u64 off = 12ULL;

    if (resp == (const u8 *)0 || out_ipv4_be == (u64 *)0 || resp_len < 12ULL) {
        return 0;
    }

    id = ush_httpget_read_be16(&resp[0]);
    if (id != expected_id) {
        return 0;
    }

    flags = ush_httpget_read_be16(&resp[2]);
    qdcount = ush_httpget_read_be16(&resp[4]);
    ancount = ush_httpget_read_be16(&resp[6]);

    if ((flags & 0x8000U) == 0U) {
        return 0;
    }

    if ((flags & 0x000FU) != 0U) {
        return 0;
    }

    for (i = 0U; i < qdcount; i++) {
        if (ush_httpget_dns_skip_name(resp, resp_len, &off) == 0) {
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

        if (ush_httpget_dns_skip_name(resp, resp_len, &off) == 0) {
            return 0;
        }
        if (off + 10ULL > resp_len) {
            return 0;
        }

        type = ush_httpget_read_be16(&resp[off + 0ULL]);
        klass = ush_httpget_read_be16(&resp[off + 2ULL]);
        rdlen = ush_httpget_read_be16(&resp[off + 8ULL]);
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

static int ush_httpget_dns_resolve_ipv4(const char *host, u64 *out_ipv4_be) {
    u64 dns_server;
    u8 query[USH_HTTPGET_DNS_PACKET_MAX];
    u8 answer[USH_HTTPGET_DNS_PACKET_MAX];
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

    txid = (u16)((cleonos_sys_timer_ticks() ^ 0x48A1ULL) & 0xFFFFULL);
    src_port = (u16)(42000U + (txid % 20000U));

    (void)memset(query, 0, sizeof(query));
    if (ush_httpget_dns_encode_qname(host, query + 12ULL, (u64)sizeof(query) - 12ULL, &qname_len) == 0) {
        return 0;
    }

    ush_httpget_write_be16(&query[0], txid);
    ush_httpget_write_be16(&query[2], 0x0100U);
    ush_httpget_write_be16(&query[4], 1U);
    ush_httpget_write_be16(&query[6], 0U);
    ush_httpget_write_be16(&query[8], 0U);
    ush_httpget_write_be16(&query[10], 0U);

    query_len = 12ULL + qname_len;
    if (query_len + 4ULL > (u64)sizeof(query)) {
        return 0;
    }

    ush_httpget_write_be16(&query[query_len], 1U);
    ush_httpget_write_be16(&query[query_len + 2ULL], 1U);
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

        if (ush_httpget_dns_parse_first_a(answer, got, txid, out_ipv4_be) != 0) {
            return 1;
        }
    }

    return 0;
}

static int ush_httpget_stdout_write(const void *buffer, u64 size) {
    const u8 *bytes = (const u8 *)buffer;
    u64 written_total = 0ULL;

    if (size == 0ULL) {
        return 1;
    }

    if (buffer == (const void *)0) {
        return 0;
    }

    while (written_total < size) {
        u64 written = cleonos_sys_fd_write(1ULL, bytes + written_total, size - written_total);

        if (written == (u64)-1 || written == 0ULL) {
            return 0;
        }

        written_total += written;
    }

    return 1;
}

static int ush_cmd_httpget(const char *arg) {
    ush_httpget_url url;
    u64 dst_ipv4_be = 0ULL;
    cleonos_net_tcp_connect_req conn_req;
    cleonos_net_tcp_send_req send_req;
    char request[1024];
    int request_len;
    u64 sent;
    u64 idle_loops = 0ULL;

    if (arg == (const char *)0 || arg[0] == '\0') {
        (void)puts("httpget: usage httpget <http://host[:port]/path>");
        return 0;
    }

    if (ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0) {
        (void)puts("usage: httpget <http://host[:port]/path>");
        (void)puts("note: https:// is not supported yet");
        return 1;
    }

    if (cleonos_sys_net_available() == 0ULL) {
        (void)puts("httpget: network unavailable");
        return 0;
    }

    if (ush_httpget_parse_url(arg, &url) == 0) {
        (void)puts("httpget: invalid URL, expected http://host[:port]/path");
        return 0;
    }

    if (ush_httpget_parse_ipv4_be(url.host, &dst_ipv4_be) == 0) {
        if (ush_httpget_dns_resolve_ipv4(url.host, &dst_ipv4_be) == 0) {
            (void)fputs("httpget: DNS resolve failed for ", 1);
            (void)puts(url.host);
            return 0;
        }
    }

    (void)fputs("httpget: connect ", 1);
    ush_httpget_print_ipv4(dst_ipv4_be);
    (void)printf(":%u\n", (unsigned int)url.port);

    ush_zero(&conn_req, (u64)sizeof(conn_req));
    conn_req.dst_ipv4_be = dst_ipv4_be;
    conn_req.dst_port = (u64)url.port;
    conn_req.src_port = 0ULL;
    conn_req.poll_budget = USH_HTTPGET_TCP_POLL_BUDGET;

    if (cleonos_sys_net_tcp_connect(&conn_req) == 0ULL) {
        (void)puts("httpget: tcp connect failed");
        return 0;
    }

    if (url.port != 80U) {
        request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: cleonos-httpget/1.0\r\nAccept: */*\r\n"
                               "Connection: close\r\n\r\n",
                               url.path, url.host, (unsigned int)url.port);
    } else {
        request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: cleonos-httpget/1.0\r\nAccept: */*\r\n"
                               "Connection: close\r\n\r\n",
                               url.path, url.host);
    }

    if (request_len <= 0 || (u64)request_len >= (u64)sizeof(request)) {
        (void)cleonos_sys_net_tcp_close(USH_HTTPGET_TCP_POLL_BUDGET);
        (void)puts("httpget: request build failed");
        return 0;
    }

    send_req.payload_ptr = (u64)(usize)request;
    send_req.payload_len = (u64)request_len;
    send_req.poll_budget = USH_HTTPGET_TCP_POLL_BUDGET;

    sent = cleonos_sys_net_tcp_send(&send_req);
    if (sent != (u64)request_len) {
        (void)cleonos_sys_net_tcp_close(USH_HTTPGET_TCP_POLL_BUDGET);
        (void)puts("httpget: tcp send failed");
        return 0;
    }

    for (;;) {
        cleonos_net_tcp_recv_req recv_req;
        u8 chunk[USH_HTTPGET_RECV_CHUNK];
        u64 got;

        recv_req.out_payload_ptr = (u64)(usize)chunk;
        recv_req.payload_capacity = (u64)sizeof(chunk);
        recv_req.poll_budget = 60000ULL;

        got = cleonos_sys_net_tcp_recv(&recv_req);
        if (got == 0ULL) {
            idle_loops++;
            if (idle_loops >= USH_HTTPGET_TCP_RECV_IDLE_LOOPS) {
                break;
            }
            (void)cleonos_sys_sleep_ticks(1ULL);
            continue;
        }

        idle_loops = 0ULL;
        if (ush_httpget_stdout_write(chunk, got) == 0) {
            (void)cleonos_sys_net_tcp_close(USH_HTTPGET_TCP_POLL_BUDGET);
            (void)puts("\nhttpget: write failed");
            return 0;
        }
    }

    (void)cleonos_sys_net_tcp_close(USH_HTTPGET_TCP_POLL_BUDGET);
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "httpget") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_httpget(arg);

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
