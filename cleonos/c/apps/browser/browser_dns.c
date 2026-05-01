#include "browser_internal.h"

#include <string.h>

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

int ush_browser_dns_resolve_ipv4(const char *host, u64 *out_ipv4_be) {
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
