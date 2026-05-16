/* Cross-platform client for CLeonOS rshd. Build examples:
 *   Linux:   cc -O2 -Wall -Wextra -o cleonos-rsh-client tools/cleonos-rsh-client.c
 *   Windows: cl /O2 /W3 tools\cleonos-rsh-client.c ws2_32.lib
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET rsh_socket_t;
#define RSH_INVALID_SOCKET INVALID_SOCKET
#define rsh_close_socket closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int rsh_socket_t;
#define RSH_INVALID_SOCKET (-1)
#define rsh_close_socket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RSH_HOST_DEFAULT_PORT 2222
#define RSH_HOST_RECV_CHUNK 1024
#define RSH_HOST_LINE_MAX 1024

#define RSH_PROMPT_CLOSED 0
#define RSH_PROMPT_SHELL 1
#define RSH_PROMPT_LOGIN 2
#define RSH_PROMPT_PASSWORD 3

static int rsh_net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) ? 1 : 0;
#else
    return 1;
#endif
}

static void rsh_net_done(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static void rsh_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    {
        struct timeval tv;
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        (void)select(0, NULL, NULL, NULL, &tv);
    }
#endif
}

static int rsh_send_all(rsh_socket_t sock, const char *data, size_t len) {
    size_t sent_total = 0;

    while (sent_total < len) {
        int sent = send(sock, data + sent_total, (int)(len - sent_total), 0);
        if (sent <= 0) {
            return 0;
        }
        sent_total += (size_t)sent;
    }
    return 1;
}

static int rsh_tail_ends_with(const char *tail, const char *suffix) {
    size_t tail_len = strlen(tail);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > tail_len) {
        return 0;
    }
    return strcmp(tail + (tail_len - suffix_len), suffix) == 0;
}

static int rsh_recv_until_prompt(rsh_socket_t sock) {
    char buf[RSH_HOST_RECV_CHUNK + 1];
    char tail[32];
    size_t tail_len = 0;

    tail[0] = '\0';

    for (;;) {
        int got = recv(sock, buf, RSH_HOST_RECV_CHUNK, 0);
        int i;
        if (got == 0) {
            return RSH_PROMPT_CLOSED;
        }
        if (got < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) {
                rsh_sleep_ms(10);
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                rsh_sleep_ms(10);
                continue;
            }
#endif
            return RSH_PROMPT_CLOSED;
        }
        buf[got] = '\0';
        fputs(buf, stdout);
        fflush(stdout);
        for (i = 0; i < got; i++) {
            if (tail_len + 1 >= sizeof(tail)) {
                memmove(tail, tail + 1, tail_len - 1);
                tail_len--;
            }
            tail[tail_len++] = buf[i];
            tail[tail_len] = '\0';

            if (rsh_tail_ends_with(tail, "$ ")) {
                return RSH_PROMPT_SHELL;
            }
            if (rsh_tail_ends_with(tail, "login: ")) {
                return RSH_PROMPT_LOGIN;
            }
            if (rsh_tail_ends_with(tail, "password: ")) {
                return RSH_PROMPT_PASSWORD;
            }
        }
    }
}

static int rsh_read_stdin_line(char *line, size_t line_size, int echo) {
    if (line == NULL || line_size == 0) {
        return 0;
    }

    if (echo) {
        return fgets(line, (int)line_size, stdin) != NULL;
    }

#ifdef _WIN32
    {
        size_t len = 0;
        for (;;) {
            int ch = _getch();
            if (ch == '\r' || ch == '\n') {
                putchar('\n');
                if (len + 1 < line_size) {
                    line[len++] = '\n';
                }
                line[len] = '\0';
                return 1;
            }
            if ((ch == '\b' || ch == 127) && len > 0) {
                len--;
                continue;
            }
            if (ch >= 0 && len + 2 < line_size) {
                line[len++] = (char)ch;
            }
        }
    }
#else
    {
        struct termios old_term;
        struct termios new_term;
        int ok;

        if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
            return fgets(line, (int)line_size, stdin) != NULL;
        }
        new_term = old_term;
        new_term.c_lflag &= (tcflag_t)~ECHO;
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term);
        ok = (fgets(line, (int)line_size, stdin) != NULL) ? 1 : 0;
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
        putchar('\n');
        return ok;
    }
#endif
}

int main(int argc, char **argv) {
    const char *host;
    int port = RSH_HOST_DEFAULT_PORT;
    rsh_socket_t sock;
    struct sockaddr_in addr;
    char line[RSH_HOST_LINE_MAX];

    if (argc < 2) {
        fprintf(stderr, "usage: cleonos-rsh-client <a.b.c.d> [port]\n");
        return 1;
    }
    host = argv[1];
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "invalid port\n");
            return 1;
        }
    }

    if (!rsh_net_init()) {
        fprintf(stderr, "network init failed\n");
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == RSH_INVALID_SOCKET) {
        fprintf(stderr, "socket failed\n");
        rsh_net_done();
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid IPv4 address\n");
        rsh_close_socket(sock);
        rsh_net_done();
        return 1;
    }

    if (connect(sock, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "connect failed\n");
        rsh_close_socket(sock);
        rsh_net_done();
        return 1;
    }

    for (;;) {
        int prompt = rsh_recv_until_prompt(sock);
        if (prompt == RSH_PROMPT_CLOSED) {
            break;
        }
        if (!rsh_read_stdin_line(line, sizeof(line), prompt != RSH_PROMPT_PASSWORD)) {
            break;
        }
        if (!rsh_send_all(sock, line, strlen(line))) {
            break;
        }
        if (strcmp(line, "exit\n") == 0 || strcmp(line, "quit\n") == 0) {
            break;
        }
    }

    rsh_close_socket(sock);
    rsh_net_done();
    return 0;
}
