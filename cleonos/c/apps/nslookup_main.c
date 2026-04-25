#include "cmd_runtime.h"
#include <stdio.h>

#define USH_DNS_QUERY_MAX 512U

typedef unsigned char u8;
typedef unsigned short u16;

static u16 ush_dns_read_be16(const u8 *ptr) {
    return (u16)(((u16)ptr[0] << 8U) | (u16)ptr[1]);
}

static void ush_dns_write_be16(u8 *ptr, u16 value) {
    ptr[0] = (u8)((value >> 8U) & 0xFFU);
    ptr[1] = (u8)(value & 0xFFU);
}

static void ush_dns_print_ipv4_be(u64 ipv4_be) {
    unsigned int a = (unsigned int)((ipv4_be >> 24ULL) & 0xFFULL);
    unsigned int b = (unsigned int)((ipv4_be >> 16ULL) & 0xFFULL);
    unsigned int c = (unsigned int)((ipv4_be >> 8ULL) & 0xFFULL);
    unsigned int d = (unsigned int)(ipv4_be & 0xFFULL);
    (void)printf("%u.%u.%u.%u", a, b, c, d);
}

static int ush_dns_encode_qname(const char *host, u8 *out, u64 out_cap, u64 *out_len) {
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

static int ush_dns_skip_name(const u8 *msg, u64 msg_len, u64 *io_off) {
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

static int ush_dns_parse_response(const u8 *resp, u64 resp_len, u16 expected_id) {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 i;
    u64 off = 12ULL;
    int found = 0;

    if (resp == (const u8 *)0 || resp_len < 12ULL) {
        return 0;
    }

    id = ush_dns_read_be16(&resp[0]);
    if (id != expected_id) {
        return 0;
    }

    flags = ush_dns_read_be16(&resp[2]);
    qdcount = ush_dns_read_be16(&resp[4]);
    ancount = ush_dns_read_be16(&resp[6]);

    if ((flags & 0x8000U) == 0U) {
        return 0;
    }

    if ((flags & 0x000FU) != 0U) {
        (void)printf("nslookup: server error rcode=%u\n", (unsigned int)(flags & 0x000FU));
        return 0;
    }

    for (i = 0U; i < qdcount; i++) {
        if (ush_dns_skip_name(resp, resp_len, &off) == 0) {
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

        if (ush_dns_skip_name(resp, resp_len, &off) == 0) {
            return found;
        }
        if (off + 10ULL > resp_len) {
            return found;
        }

        type = ush_dns_read_be16(&resp[off + 0ULL]);
        klass = ush_dns_read_be16(&resp[off + 2ULL]);
        rdlen = ush_dns_read_be16(&resp[off + 8ULL]);
        off += 10ULL;

        if (off + (u64)rdlen > resp_len) {
            return found;
        }

        if (type == 1U && klass == 1U && rdlen == 4U) {
            u64 ip = ((u64)resp[off] << 24ULL) | ((u64)resp[off + 1ULL] << 16ULL) | ((u64)resp[off + 2ULL] << 8ULL) |
                     (u64)resp[off + 3ULL];
            ush_dns_print_ipv4_be(ip);
            (void)putchar('\n');
            found++;
        }

        off += (u64)rdlen;
    }

    return found;
}

static int ush_cmd_nslookup(const char *arg) {
    u64 dns_server;
    u8 query[USH_DNS_QUERY_MAX];
    u8 answer[USH_DNS_QUERY_MAX];
    u64 qname_len = 0ULL;
    u64 query_len;
    u16 txid;
    u16 src_port;
    cleonos_net_udp_send_req send_req;
    int loops;

    if (arg == (const char *)0 || arg[0] == '\0') {
        (void)puts("nslookup: usage nslookup <domain>");
        return 0;
    }

    if (cleonos_sys_net_available() == 0ULL) {
        (void)puts("nslookup: network unavailable");
        return 0;
    }

    dns_server = cleonos_sys_net_dns_server();
    if (dns_server == 0ULL) {
        (void)puts("nslookup: dns server unavailable");
        return 0;
    }

    txid = (u16)((cleonos_sys_timer_ticks() ^ 0xA5C3ULL) & 0xFFFFULL);
    src_port = (u16)(40000U + (txid % 20000U));

    (void)memset(query, 0, sizeof(query));
    if (ush_dns_encode_qname(arg, query + 12ULL, (u64)sizeof(query) - 12ULL, &qname_len) == 0) {
        (void)puts("nslookup: invalid domain");
        return 0;
    }
    ush_dns_write_be16(&query[0], txid);
    ush_dns_write_be16(&query[2], 0x0100U);
    ush_dns_write_be16(&query[4], 1U);
    ush_dns_write_be16(&query[6], 0U);
    ush_dns_write_be16(&query[8], 0U);
    ush_dns_write_be16(&query[10], 0U);

    query_len = 12ULL + qname_len;
    if (query_len + 4ULL > (u64)sizeof(query)) {
        (void)puts("nslookup: query too large");
        return 0;
    }
    ush_dns_write_be16(&query[query_len], 1U);
    ush_dns_write_be16(&query[query_len + 2ULL], 1U);
    query_len += 4ULL;

    send_req.dst_ipv4_be = dns_server;
    send_req.dst_port = 53ULL;
    send_req.src_port = (u64)src_port;
    send_req.payload_ptr = (u64)(usize)query;
    send_req.payload_len = query_len;

    if (cleonos_sys_net_udp_send(&send_req) != query_len) {
        (void)puts("nslookup: send failed");
        return 0;
    }

    (void)fputs("Server: ", 1);
    ush_dns_print_ipv4_be(dns_server);
    (void)putchar('\n');
    (void)fputs("Query: ", 1);
    (void)puts(arg);
    (void)puts("Answer:");

    for (loops = 0; loops < 600; loops++) {
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

        if (ush_dns_parse_response(answer, got, txid) > 0) {
            return 1;
        }
    }

    (void)puts("nslookup: timeout");
    return 0;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "nslookup") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_nslookup(arg);

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
