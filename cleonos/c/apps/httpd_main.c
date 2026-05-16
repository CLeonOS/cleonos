#include "cmd_runtime.h"
#include <stdio.h>
#include <string.h>

#define HTTPD_DEFAULT_PORT 80ULL
#define HTTPD_DEFAULT_ROOT "/www"
#define HTTPD_POLL_BUDGET 200000000ULL
#define HTTPD_REQ_MAX 4096U
#define HTTPD_FILE_CHUNK 2048U

static int httpd_starts_with(const char *text, const char *prefix) {
    u64 i = 0ULL;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static const char *httpd_content_type(const char *path) {
    const char *dot = strrchr(path, '.');

    if (dot == (const char *)0) {
        return "application/octet-stream";
    }

    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) {
        return "text/html; charset=utf-8";
    }
    if (strcmp(dot, ".txt") == 0 || strcmp(dot, ".log") == 0) {
        return "text/plain; charset=utf-8";
    }
    if (strcmp(dot, ".css") == 0) {
        return "text/css; charset=utf-8";
    }
    if (strcmp(dot, ".js") == 0) {
        return "application/javascript";
    }
    if (strcmp(dot, ".png") == 0) {
        return "image/png";
    }
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
        return "image/jpeg";
    }
    if (strcmp(dot, ".gif") == 0) {
        return "image/gif";
    }

    return "application/octet-stream";
}

static int httpd_send_all(const char *data, u64 len) {
    u64 sent_total = 0ULL;

    while (sent_total < len) {
        cleonos_net_tcp_send_req req;
        u64 sent;

        req.payload_ptr = (u64)(usize)(data + sent_total);
        req.payload_len = len - sent_total;
        req.poll_budget = HTTPD_POLL_BUDGET;
        sent = cleonos_sys_net_tcp_send(&req);
        if (sent == 0ULL) {
            return 0;
        }
        sent_total += sent;
    }

    return 1;
}

static int httpd_send_text_response(int code, const char *reason, const char *body) {
    char header[256];
    u64 body_len = (body != (const char *)0) ? (u64)strlen(body) : 0ULL;
    int header_len;

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Server: cleonos-httpd/0.1\r\n"
                          "Content-Type: text/plain; charset=utf-8\r\n"
                          "Content-Length: %llu\r\n"
                          "Connection: close\r\n\r\n",
                          code, reason, (unsigned long long)body_len);
    if (header_len <= 0 || (u64)header_len >= (u64)sizeof(header)) {
        return 0;
    }

    if (httpd_send_all(header, (u64)header_len) == 0) {
        return 0;
    }
    if (body_len > 0ULL && httpd_send_all(body, body_len) == 0) {
        return 0;
    }

    return 1;
}

static int httpd_safe_path(const char *path) {
    u64 i;

    if (path == (const char *)0 || path[0] != '/') {
        return 0;
    }

    for (i = 0ULL; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            return 0;
        }
        if (path[i] == '.' && path[i + 1ULL] == '.') {
            return 0;
        }
    }

    return 1;
}

static int httpd_build_file_path(char *out, u64 out_size, const char *root, const char *url_path) {
    const char *suffix = url_path;

    if (out == (char *)0 || root == (const char *)0 || url_path == (const char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (httpd_safe_path(url_path) == 0) {
        return 0;
    }

    if (strcmp(url_path, "/") == 0) {
        suffix = "/index.html";
    }

    return ((u64)snprintf(out, (unsigned long)out_size, "%s%s", root, suffix) < out_size) ? 1 : 0;
}

static int httpd_send_file(const char *path) {
    char header[256];
    char buf[HTTPD_FILE_CHUNK];
    u64 size;
    u64 fd;
    int header_len;

    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1 || cleonos_sys_fs_stat_type(path) != 1ULL) {
        return httpd_send_text_response(404, "Not Found", "not found\n");
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        return httpd_send_text_response(403, "Forbidden", "open failed\n");
    }

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 200 OK\r\n"
                          "Server: cleonos-httpd/0.1\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %llu\r\n"
                          "Connection: close\r\n\r\n",
                          httpd_content_type(path), (unsigned long long)size);
    if (header_len <= 0 || (u64)header_len >= (u64)sizeof(header) ||
        httpd_send_all(header, (u64)header_len) == 0) {
        (void)cleonos_sys_fd_close(fd);
        return 0;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            return 0;
        }
        if (got == 0ULL) {
            break;
        }
        if (httpd_send_all(buf, got) == 0) {
            (void)cleonos_sys_fd_close(fd);
            return 0;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

static int httpd_parse_request_path(char *req, char *out_path, u64 out_size) {
    char *space;
    char *end;
    u64 len;

    if (req == (char *)0 || out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    if (httpd_starts_with(req, "GET ") == 0) {
        return 0;
    }

    space = req + 4;
    end = strchr(space, ' ');
    if (end == (char *)0) {
        return 0;
    }

    len = (u64)(end - space);
    if (len == 0ULL || len >= out_size) {
        return 0;
    }

    memcpy(out_path, space, (size_t)len);
    out_path[len] = '\0';
    return 1;
}

static int httpd_strip_query(char *path) {
    char *query;

    if (path == (char *)0) {
        return 0;
    }

    query = strchr(path, '?');
    if (query != (char *)0) {
        *query = '\0';
    }
    return 1;
}

static int httpd_handle_client(const char *root) {
    char req_buf[HTTPD_REQ_MAX + 1U];
    char url_path[USH_PATH_MAX];
    char file_path[USH_PATH_MAX];
    u64 total = 0ULL;

    for (;;) {
        cleonos_net_tcp_recv_req req;
        u64 got;

        if (total >= HTTPD_REQ_MAX) {
            return httpd_send_text_response(413, "Payload Too Large", "request too large\n");
        }

        req.out_payload_ptr = (u64)(usize)(req_buf + total);
        req.payload_capacity = (u64)HTTPD_REQ_MAX - total;
        req.poll_budget = HTTPD_POLL_BUDGET / 8ULL;
        got = cleonos_sys_net_tcp_recv(&req);
        if (got == 0ULL) {
            break;
        }

        total += got;
        req_buf[total] = '\0';
        if (strstr(req_buf, "\r\n\r\n") != (char *)0 || strstr(req_buf, "\n\n") != (char *)0) {
            break;
        }
    }

    if (total == 0ULL) {
        return 0;
    }

    req_buf[total] = '\0';
    if (httpd_parse_request_path(req_buf, url_path, (u64)sizeof(url_path)) == 0) {
        return httpd_send_text_response(405, "Method Not Allowed", "only GET is supported\n");
    }

    (void)httpd_strip_query(url_path);
    if (httpd_build_file_path(file_path, (u64)sizeof(file_path), root, url_path) == 0) {
        return httpd_send_text_response(400, "Bad Request", "bad path\n");
    }

    return httpd_send_file(file_path);
}

static u64 httpd_parse_port(const char *text) {
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

int cleonos_app_main(int argc, char **argv, char **envp) {
    const char *root = HTTPD_DEFAULT_ROOT;
    u64 port = HTTPD_DEFAULT_PORT;
    u64 max_requests = 0ULL;
    u64 served = 0ULL;
    int i;

    (void)envp;

    for (i = 1; i < argc; i++) {
        if (argv[i] == (char *)0) {
            continue;
        }
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = httpd_parse_port(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_requests = httpd_parse_port(argv[++i]);
        } else if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0) {
            puts("usage: httpd [-p port] [-r root] [-n max_requests]");
            puts("default: httpd -p 80 -r /www");
            return 0;
        } else {
            puts("httpd: invalid argument");
            puts("usage: httpd [-p port] [-r root] [-n max_requests]");
            return 1;
        }
    }

    if (port == 0ULL) {
        puts("httpd: invalid port");
        return 1;
    }

    if (cleonos_sys_net_available() == 0ULL) {
        puts("httpd: network unavailable");
        return 1;
    }

    printf("httpd: serving %s on port %llu\n", root, (unsigned long long)port);
    for (;;) {
        cleonos_net_tcp_listen_req listen_req;
        cleonos_net_tcp_accept_req accept_req;

        listen_req.port = port;
        if (cleonos_sys_net_tcp_listen(&listen_req) == 0ULL) {
            puts("httpd: listen failed");
            return 1;
        }

        accept_req.poll_budget = HTTPD_POLL_BUDGET;
        if (cleonos_sys_net_tcp_accept(&accept_req) == 0ULL) {
            continue;
        }

        (void)httpd_handle_client(root);
        (void)cleonos_sys_net_tcp_close(HTTPD_POLL_BUDGET);
        served++;

        if (max_requests != 0ULL && served >= max_requests) {
            break;
        }
    }

    return 0;
}
