#include "cmd_runtime.h"
#include <stdio.h>
#include <string.h>

#define RSH_DEFAULT_PORT 2222ULL
#define RSH_POLL_BUDGET 200000000ULL
#define RSH_RECV_CHUNK 512ULL
#define RSH_LINE_MAX 256ULL
#define RSH_IDLE_SLEEP_MS 10ULL

#define RSH_PROMPT_CLOSED 0
#define RSH_PROMPT_SHELL 1
#define RSH_PROMPT_LOGIN 2
#define RSH_PROMPT_PASSWORD 3

static int rsh_parse_ipv4_be(const char *text, u64 *out_ipv4_be) {
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

static u64 rsh_parse_port(const char *text) {
    u64 value = 0ULL;
    u64 i;

    if (text == (const char *)0 || text[0] == '\0') {
        return 0ULL;
    }
    for (i = 0ULL; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0ULL;
        }
        value = value * 10ULL + (u64)(text[i] - '0');
        if (value > 65535ULL) {
            return 0ULL;
        }
    }
    return value;
}

static int rsh_send_all(const char *data, u64 len) {
    u64 sent_total = 0ULL;

    while (sent_total < len) {
        cleonos_net_tcp_send_req req;
        u64 sent;

        req.payload_ptr = (u64)(usize)(data + sent_total);
        req.payload_len = len - sent_total;
        req.poll_budget = RSH_POLL_BUDGET;
        sent = cleonos_sys_net_tcp_send(&req);
        if (sent == 0ULL) {
            return 0;
        }
        sent_total += sent;
    }
    return 1;
}

static int rsh_tail_ends_with(const char *tail, const char *suffix) {
    u64 tail_len = (u64)strlen(tail);
    u64 suffix_len = (u64)strlen(suffix);

    if (suffix_len > tail_len) {
        return 0;
    }
    return (strcmp(tail + (tail_len - suffix_len), suffix) == 0) ? 1 : 0;
}

static int rsh_recv_until_prompt(void) {
    char buf[RSH_RECV_CHUNK + 1U];
    char tail[32];
    u64 tail_len = 0ULL;

    tail[0] = '\0';

    for (;;) {
        cleonos_net_tcp_recv_req req;
        u64 got;
        u64 i;

        req.out_payload_ptr = (u64)(usize)buf;
        req.payload_capacity = RSH_RECV_CHUNK;
        req.poll_budget = RSH_POLL_BUDGET;
        got = cleonos_sys_net_tcp_recv(&req);
        if (got == 0ULL) {
            (void)cleonos_sys_sleep_ms(RSH_IDLE_SLEEP_MS);
            continue;
        }
        buf[got] = '\0';
        (void)fputs(buf, 1);

        for (i = 0ULL; i < got; i++) {
            if (tail_len + 1ULL >= (u64)sizeof(tail)) {
                (void)memmove(tail, tail + 1, (size_t)(tail_len - 1ULL));
                tail_len--;
            }
            tail[tail_len++] = buf[i];
            tail[tail_len] = '\0';

            if (rsh_tail_ends_with(tail, "$ ") != 0) {
                return RSH_PROMPT_SHELL;
            }
            if (rsh_tail_ends_with(tail, "login: ") != 0) {
                return RSH_PROMPT_LOGIN;
            }
            if (rsh_tail_ends_with(tail, "password: ") != 0) {
                return RSH_PROMPT_PASSWORD;
            }
        }
    }
}

static int rsh_read_line(char *out, u64 out_size, int echo) {
    u64 pos = 0ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0;
    }
    out[0] = '\0';

    for (;;) {
        int c = getchar();
        if (c == EOF) {
            return 0;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            putchar('\n');
            out[pos++] = '\n';
            out[pos] = '\0';
            return 1;
        }
        if (pos + 2ULL < out_size) {
            out[pos++] = (char)c;
            if (echo != 0) {
                putchar(c);
            }
        }
    }
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    const char *host;
    u64 port = RSH_DEFAULT_PORT;
    u64 ipv4_be;
    cleonos_net_tcp_connect_req conn;
    char line[RSH_LINE_MAX];

    (void)envp;
    if (argc < 2 || argv == (char **)0 || argv[1] == (char *)0) {
        puts("usage: rsh <a.b.c.d> [port]");
        return 1;
    }
    host = argv[1];
    if (argc >= 3 && argv[2] != (char *)0) {
        port = rsh_parse_port(argv[2]);
        if (port == 0ULL) {
            puts("rsh: invalid port");
            return 1;
        }
    }
    if (rsh_parse_ipv4_be(host, &ipv4_be) == 0) {
        puts("rsh: invalid IPv4 address");
        return 1;
    }

    conn.dst_ipv4_be = ipv4_be;
    conn.dst_port = port;
    conn.src_port = 0ULL;
    conn.poll_budget = RSH_POLL_BUDGET;
    if (cleonos_sys_net_tcp_connect(&conn) == 0ULL) {
        printf("rsh: connect failed: %llu\n", (unsigned long long)cleonos_sys_net_tcp_last_error());
        return 1;
    }

    for (;;) {
        int prompt = rsh_recv_until_prompt();
        if (prompt == RSH_PROMPT_CLOSED) {
            break;
        }
        if (rsh_read_line(line, (u64)sizeof(line), (prompt == RSH_PROMPT_PASSWORD) ? 0 : 1) == 0) {
            break;
        }
        if (rsh_send_all(line, (u64)strlen(line)) == 0) {
            break;
        }
        if (strcmp(line, "exit\n") == 0 || strcmp(line, "quit\n") == 0) {
            (void)rsh_recv_until_prompt();
            break;
        }
    }

    (void)cleonos_sys_net_tcp_close(RSH_POLL_BUDGET);
    return 0;
}
