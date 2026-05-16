#include "cmd_runtime.h"
#include "../pkg/pkg_internal.h"
#include <stdio.h>
#include <string.h>

#define WEBCONSOLE_DEFAULT_PORT 8080ULL
#define WEBCONSOLE_POLL_BUDGET 200000000ULL
#define WEBCONSOLE_REQ_MAX 4096U
#define WEBCONSOLE_HTML_MAX 131072U
#define WEBCONSOLE_LOG_PAGE_SIZE 64ULL
#define WEBCONSOLE_FILE_PREVIEW_MAX 8192U

typedef struct wc_html {
    char *buf;
    u64 cap;
    u64 len;
    int truncated;
} wc_html;

typedef struct wc_query {
    char view[32];
    char action[32];
    char path[USH_PATH_MAX];
    char q[96];
    char name[PKG_NAME_MAX];
    u64 page;
} wc_query;

typedef struct wc_pkg_ctx {
    wc_html *html;
} wc_pkg_ctx;

static int wc_starts_with(const char *text, const char *prefix) {
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

static int wc_send_all(const char *data, u64 len) {
    u64 sent_total = 0ULL;

    while (sent_total < len) {
        cleonos_net_tcp_send_req req;
        u64 sent;

        req.payload_ptr = (u64)(usize)(data + sent_total);
        req.payload_len = len - sent_total;
        req.poll_budget = WEBCONSOLE_POLL_BUDGET;
        sent = cleonos_sys_net_tcp_send(&req);
        if (sent == 0ULL) {
            return 0;
        }
        sent_total += sent;
    }
    return 1;
}

static int wc_send_response(int code, const char *reason, const char *type, const char *body, u64 body_len) {
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Server: cleonos-webconsole/0.1\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %llu\r\n"
                              "Connection: close\r\n\r\n",
                              code, reason, type, (unsigned long long)body_len);

    if (header_len <= 0 || (u64)header_len >= (u64)sizeof(header)) {
        return 0;
    }
    if (wc_send_all(header, (u64)header_len) == 0) {
        return 0;
    }
    if (body_len > 0ULL && body != (const char *)0 && wc_send_all(body, body_len) == 0) {
        return 0;
    }
    return 1;
}

static int wc_send_text(int code, const char *reason, const char *body) {
    return wc_send_response(code, reason, "text/plain; charset=utf-8", body,
                            body != (const char *)0 ? (u64)strlen(body) : 0ULL);
}

static void wc_init(wc_html *h, char *buf, u64 cap) {
    h->buf = buf;
    h->cap = cap;
    h->len = 0ULL;
    h->truncated = 0;
    if (buf != (char *)0 && cap > 0ULL) {
        buf[0] = '\0';
    }
}

static void wc_ch(wc_html *h, char ch) {
    if (h == (wc_html *)0 || h->buf == (char *)0 || h->cap == 0ULL) {
        return;
    }
    if (h->len + 1ULL >= h->cap) {
        h->truncated = 1;
        return;
    }
    h->buf[h->len++] = ch;
    h->buf[h->len] = '\0';
}

static void wc_raw(wc_html *h, const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        wc_ch(h, text[i++]);
        if (h->truncated != 0) {
            return;
        }
    }
}

static void wc_esc(wc_html *h, const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        if (text[i] == '&') {
            wc_raw(h, "&amp;");
        } else if (text[i] == '<') {
            wc_raw(h, "&lt;");
        } else if (text[i] == '>') {
            wc_raw(h, "&gt;");
        } else if (text[i] == '"') {
            wc_raw(h, "&quot;");
        } else if (text[i] == '\'') {
            wc_raw(h, "&#39;");
        } else {
            wc_ch(h, text[i]);
        }
        i++;
    }
}

static void wc_u64(wc_html *h, u64 value) {
    char tmp[32];
    u64 len = 0ULL;

    if (value == 0ULL) {
        wc_ch(h, '0');
        return;
    }
    while (value != 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }
    while (len > 0ULL) {
        wc_ch(h, tmp[--len]);
    }
}

static void wc_link(wc_html *h, const char *href, const char *label) {
    wc_raw(h, "<a href=\"");
    wc_esc(h, href);
    wc_raw(h, "\">");
    wc_esc(h, label);
    wc_raw(h, "</a>");
}

static void wc_begin(wc_html *h, const char *title) {
    wc_raw(h,
           "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
           "content=\"width=device-width, initial-scale=1\"><title>");
    wc_esc(h, title);
    wc_raw(h,
           "</title><style>"
           "body{font-family:sans-serif;margin:12px;}"
           "nav a{margin-right:10px;}"
           "table{border-collapse:collapse;width:100%;}"
           "td,th{border:1px solid #999;padding:3px;}"
           "pre{white-space:pre-wrap;border:1px solid #999;padding:6px;}"
           ".ok{color:green}.err{color:red}"
           "</style></head><body><h1>");
    wc_esc(h, title);
    wc_raw(h, "</h1><main>");
}

static void wc_end(wc_html *h) {
    if (h->truncated != 0) {
        wc_raw(h, "<p class=\"err\">page truncated</p>");
    }
    wc_raw(h, "</main></body></html>");
}

static void wc_section(wc_html *h, const char *title) {
    wc_raw(h, "<h2>");
    wc_esc(h, title);
    wc_raw(h, "</h2>");
}

static void wc_kv(wc_html *h, const char *key, const char *value) {
    wc_raw(h, "<div><b>");
    wc_esc(h, key);
    wc_raw(h, ":</b> ");
    wc_esc(h, value != (const char *)0 ? value : "");
    wc_raw(h, "</div>");
}

static void wc_kv_u64(wc_html *h, const char *key, u64 value) {
    wc_raw(h, "<div><b>");
    wc_esc(h, key);
    wc_raw(h, ":</b> ");
    wc_u64(h, value);
    wc_raw(h, "</div>");
}

static void wc_nav(wc_html *h) {
    wc_raw(h, "<nav>");
    wc_link(h, "/", "Home");
    wc_link(h, "/?view=status", "System");
    wc_link(h, "/?view=files&path=/", "Files");
    wc_link(h, "/?view=logs", "Logs");
    wc_link(h, "/?view=net", "Network");
    wc_link(h, "/?view=pkg", "Pkg");
    wc_raw(h, "</nav>");
}

static int wc_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static void wc_url_decode(char *dst, u64 dst_size, const char *src) {
    u64 di = 0ULL;
    u64 si = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL) {
        return;
    }
    dst[0] = '\0';
    if (src == (const char *)0) {
        return;
    }
    while (src[si] != '\0' && di + 1ULL < dst_size) {
        if (src[si] == '%' && wc_hex_value(src[si + 1ULL]) >= 0 && wc_hex_value(src[si + 2ULL]) >= 0) {
            int hi = wc_hex_value(src[si + 1ULL]);
            int lo = wc_hex_value(src[si + 2ULL]);
            dst[di++] = (char)((hi << 4) | lo);
            si += 3ULL;
        } else if (src[si] == '+') {
            dst[di++] = ' ';
            si++;
        } else {
            dst[di++] = src[si++];
        }
    }
    dst[di] = '\0';
}

static int wc_urlenc(wc_html *h, const char *text) {
    u64 i;
    const char *hex = "0123456789ABCDEF";

    if (text == (const char *)0) {
        return 0;
    }
    for (i = 0ULL; text[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)text[i];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '/' || ch == '~') {
            wc_ch(h, (char)ch);
        } else {
            wc_ch(h, '%');
            wc_ch(h, hex[(ch >> 4) & 0xFU]);
            wc_ch(h, hex[ch & 0xFU]);
        }
    }
    return h->truncated == 0;
}

static void wc_query_value(const char *query, const char *key, char *out, u64 out_size) {
    u64 key_len;
    const char *pos;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    out[0] = '\0';
    if (query == (const char *)0 || key == (const char *)0) {
        return;
    }
    key_len = (u64)strlen(key);
    pos = query;
    while (*pos != '\0') {
        if ((pos == query || pos[-1] == '&') && strncmp(pos, key, (size_t)key_len) == 0 && pos[key_len] == '=') {
            char encoded[USH_PATH_MAX];
            const char *value = pos + key_len + 1ULL;
            u64 len = 0ULL;

            while (value[len] != '\0' && value[len] != '&' && len + 1ULL < (u64)sizeof(encoded)) {
                encoded[len] = value[len];
                len++;
            }
            encoded[len] = '\0';
            wc_url_decode(out, out_size, encoded);
            return;
        }
        pos++;
    }
}

static u64 wc_parse_u64_default(const char *text, u64 fallback) {
    u64 value = 0ULL;
    u64 i;

    if (text == (const char *)0 || text[0] == '\0') {
        return fallback;
    }
    for (i = 0ULL; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return fallback;
        }
        value = value * 10ULL + (u64)(text[i] - '0');
    }
    return value;
}

static int wc_safe_path(const char *path) {
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

static void wc_parse_query(const char *req_path, wc_query *out) {
    const char *query = strchr(req_path, '?');
    char page[32];

    ush_zero(out, (u64)sizeof(*out));
    ush_copy(out->view, (u64)sizeof(out->view), "status");
    ush_copy(out->path, (u64)sizeof(out->path), "/");
    if (query == (const char *)0) {
        return;
    }
    query++;
    wc_query_value(query, "view", out->view, (u64)sizeof(out->view));
    wc_query_value(query, "action", out->action, (u64)sizeof(out->action));
    wc_query_value(query, "path", out->path, (u64)sizeof(out->path));
    wc_query_value(query, "q", out->q, (u64)sizeof(out->q));
    wc_query_value(query, "name", out->name, (u64)sizeof(out->name));
    wc_query_value(query, "page", page, (u64)sizeof(page));
    out->page = wc_parse_u64_default(page, 0ULL);
    if (out->view[0] == '\0') {
        ush_copy(out->view, (u64)sizeof(out->view), "status");
    }
    if (out->path[0] == '\0') {
        ush_copy(out->path, (u64)sizeof(out->path), "/");
    }
}

static void wc_parent_path(const char *path, char *out, u64 out_size) {
    u64 len;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    ush_copy(out, out_size, "/");
    if (path == (const char *)0 || strcmp(path, "/") == 0) {
        return;
    }
    len = (u64)strlen(path);
    while (len > 1ULL && path[len - 1ULL] == '/') {
        len--;
    }
    while (len > 1ULL && path[len - 1ULL] != '/') {
        len--;
    }
    if (len <= 1ULL) {
        return;
    }
    if (len >= out_size) {
        len = out_size - 1ULL;
    }
    memcpy(out, path, (size_t)len);
    out[len] = '\0';
}

static int wc_join_path(char *out, u64 out_size, const char *dir, const char *name) {
    int ret;

    if (out == (char *)0 || out_size == 0ULL || dir == (const char *)0 || name == (const char *)0) {
        return 0;
    }
    if (strcmp(dir, "/") == 0) {
        ret = snprintf(out, (usize)out_size, "/%s", name);
    } else {
        ret = snprintf(out, (usize)out_size, "%s/%s", dir, name);
    }
    return (ret > 0 && (u64)ret < out_size) ? 1 : 0;
}

static void wc_render_status(wc_html *h) {
    cleonos_sysinfo info;

    ush_zero(&info, (u64)sizeof(info));
    (void)cleonos_sys_sysinfo(&info);
    wc_section(h, "System Status");
    wc_kv(h, "Kernel", info.kernel_name);
    wc_kv(h, "Version", info.kernel_version);
    wc_kv(h, "Arch", info.arch);
    wc_kv(h, "Boot Mode", info.boot_mode);
    wc_kv_u64(h, "Uptime ms", info.uptime_ms);
    wc_kv_u64(h, "Timer ticks", info.timer_ticks);
    wc_kv_u64(h, "Timer Hz", info.timer_hz);
    wc_kv_u64(h, "Heap total", info.heap_total_bytes);
    wc_kv_u64(h, "Heap used", info.heap_used_bytes);
    wc_kv_u64(h, "Heap free", info.heap_free_bytes);
    wc_kv_u64(h, "Tasks", info.task_count);
    wc_kv_u64(h, "Services", info.service_count);
    wc_kv_u64(h, "Services ready", info.service_ready_count);
}

static void wc_render_net(wc_html *h) {
    u64 available = cleonos_sys_net_available();

    wc_section(h, "Network");
    wc_kv(h, "Available", available != 0ULL ? "yes" : "no");
    if (available == 0ULL) {
        return;
    }
    wc_kv_u64(h, "IPv4", cleonos_sys_net_ipv4_addr());
    wc_kv_u64(h, "Netmask", cleonos_sys_net_netmask());
    wc_kv_u64(h, "Gateway", cleonos_sys_net_gateway());
    wc_kv_u64(h, "DNS", cleonos_sys_net_dns_server());
}

static void wc_render_logs(wc_html *h, const wc_query *query) {
    u64 total = cleonos_sys_log_journal_count();
    u64 match_total = 0ULL;
    u64 pages;
    u64 page = query->page;
    u64 first_match;
    u64 after_last_match;
    u64 emitted = 0ULL;
    u64 i;

    for (i = 0ULL; i < total; i++) {
        char line[256];
        if (cleonos_sys_log_journal_read(i, line, (u64)sizeof(line)) != 0ULL &&
            (query->q[0] == '\0' || strstr(line, query->q) != (char *)0)) {
            match_total++;
        }
    }

    pages = (match_total + WEBCONSOLE_LOG_PAGE_SIZE - 1ULL) / WEBCONSOLE_LOG_PAGE_SIZE;
    if (pages == 0ULL) {
        pages = 1ULL;
    }
    if (page >= pages) {
        page = pages - 1ULL;
    }
    first_match = page * WEBCONSOLE_LOG_PAGE_SIZE;
    after_last_match = first_match + WEBCONSOLE_LOG_PAGE_SIZE;

    wc_section(h, "Logs");
    wc_raw(h, "<form><input type=\"hidden\" name=\"view\" value=\"logs\"><input name=\"q\" value=\"");
    wc_esc(h, query->q);
    wc_raw(h, "\" placeholder=\"search logs\"><button>Search</button></form><p>");
    if (page > 0ULL) {
        wc_raw(h, "<a href=\"/?view=logs&page=");
        wc_u64(h, page - 1ULL);
        if (query->q[0] != '\0') {
            wc_raw(h, "&q=");
            (void)wc_urlenc(h, query->q);
        }
        wc_raw(h, "\">Prev</a> ");
    }
    wc_raw(h, "Page ");
    wc_u64(h, page + 1ULL);
    wc_raw(h, " / ");
    wc_u64(h, pages);
    wc_raw(h, " (matches ");
    wc_u64(h, match_total);
    wc_raw(h, " / logs ");
    wc_u64(h, total);
    wc_raw(h, ")");
    if (page + 1ULL < pages) {
        wc_raw(h, " <a href=\"/?view=logs&page=");
        wc_u64(h, page + 1ULL);
        if (query->q[0] != '\0') {
            wc_raw(h, "&q=");
            (void)wc_urlenc(h, query->q);
        }
        wc_raw(h, "\">Next</a>");
    }
    wc_raw(h, "</p><pre>");

    for (i = 0ULL; i < total; i++) {
        char line[256];
        if (cleonos_sys_log_journal_read(i, line, (u64)sizeof(line)) != 0ULL &&
            (query->q[0] == '\0' || strstr(line, query->q) != (char *)0)) {
            if (emitted >= first_match && emitted < after_last_match) {
                wc_esc(h, line);
                wc_ch(h, '\n');
            }
            emitted++;
        }
    }
    if (total == 0ULL) {
        wc_raw(h, "(journal empty)");
    } else if (match_total == 0ULL) {
        wc_raw(h, "(no matching logs)");
    }
    wc_raw(h, "</pre>");
}

static void wc_send_file_download(const char *path) {
    char header[256];
    char buf[2048];
    u64 fd;
    u64 size;
    int header_len;

    if (wc_safe_path(path) == 0 || cleonos_sys_fs_stat_type(path) != 1ULL) {
        (void)wc_send_text(404, "Not Found", "not found\n");
        return;
    }
    size = cleonos_sys_fs_stat_size(path);
    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        (void)wc_send_text(403, "Forbidden", "open failed\n");
        return;
    }
    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 200 OK\r\nServer: cleonos-webconsole/0.1\r\n"
                          "Content-Type: application/octet-stream\r\nContent-Length: %llu\r\n"
                          "Connection: close\r\n\r\n",
                          (unsigned long long)size);
    if (header_len > 0 && (u64)header_len < (u64)sizeof(header) && wc_send_all(header, (u64)header_len) != 0) {
        for (;;) {
            u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
            if (got == 0ULL || got == (u64)-1) {
                break;
            }
            if (wc_send_all(buf, got) == 0) {
                break;
            }
        }
    }
    (void)cleonos_sys_fd_close(fd);
}

static void wc_render_file_preview(wc_html *h, const char *path) {
    char data[WEBCONSOLE_FILE_PREVIEW_MAX + 1U];
    u64 fd;
    u64 got;

    wc_section(h, "File Preview");
    if (wc_safe_path(path) == 0 || cleonos_sys_fs_stat_type(path) != 1ULL) {
        wc_raw(h, "<p>not a readable file</p>");
        return;
    }
    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        wc_raw(h, "<p>open failed</p>");
        return;
    }
    got = cleonos_sys_fd_read(fd, data, (u64)WEBCONSOLE_FILE_PREVIEW_MAX);
    (void)cleonos_sys_fd_close(fd);
    if (got == (u64)-1) {
        wc_raw(h, "<p>read failed</p>");
        return;
    }
    data[got] = '\0';
    wc_raw(h, "<pre>");
    wc_esc(h, data);
    wc_raw(h, "</pre>");
}

static void wc_render_files(wc_html *h, const wc_query *query) {
    const char *dir = query->path;
    char parent[USH_PATH_MAX];
    u64 type;
    u64 count;
    u64 i;

    wc_section(h, "Files");
    if (wc_safe_path(dir) == 0) {
        wc_raw(h, "<p>bad path</p>");
        return;
    }
    if (strcmp(query->action, "delete") == 0 && strcmp(query->path, "/") != 0) {
        wc_raw(h, cleonos_sys_fs_remove(query->path) != 0ULL ? "<p class=\"ok\">deleted: "
                                                            : "<p class=\"err\">delete failed: ");
        wc_esc(h, query->path);
        wc_raw(h, "</p>");
        wc_parent_path(query->path, parent, (u64)sizeof(parent));
        dir = parent;
    }

    type = cleonos_sys_fs_stat_type(dir);
    if (type == 1ULL && strcmp(query->action, "view") == 0) {
        wc_render_file_preview(h, dir);
        wc_parent_path(dir, parent, (u64)sizeof(parent));
        dir = parent;
        type = cleonos_sys_fs_stat_type(dir);
    }
    if (type != 2ULL) {
        wc_raw(h, "<p>not a directory</p>");
        return;
    }

    wc_raw(h, "<p>Path: <strong>");
    wc_esc(h, dir);
    wc_raw(h, "</strong></p>");
    wc_parent_path(dir, parent, (u64)sizeof(parent));
    wc_raw(h, "<p><a href=\"/?view=files&path=");
    (void)wc_urlenc(h, parent);
    wc_raw(h, "\">Up</a></p><table><tr>"
              "<th align=\"left\">Name</th><th align=\"left\">Type</th><th align=\"right\">Size</th>"
              "<th align=\"left\">Actions</th></tr>");

    count = cleonos_sys_fs_child_count(dir);
    for (i = 0ULL; i < count; i++) {
        char name[USH_PATH_MAX];
        char child[USH_PATH_MAX];
        u64 child_type;
        u64 size;

        if (cleonos_sys_fs_get_child_name(dir, i, name) == 0ULL ||
            wc_join_path(child, (u64)sizeof(child), dir, name) == 0) {
            continue;
        }
        child_type = cleonos_sys_fs_stat_type(child);
        size = cleonos_sys_fs_stat_size(child);
        wc_raw(h, "<tr><td>");
        if (child_type == 2ULL) {
            wc_raw(h, "<a href=\"/?view=files&path=");
            (void)wc_urlenc(h, child);
            wc_raw(h, "\">");
            wc_esc(h, name);
            wc_raw(h, "</a>");
        } else {
            wc_raw(h, "<a href=\"/?view=files&action=view&path=");
            (void)wc_urlenc(h, child);
            wc_raw(h, "\">");
            wc_esc(h, name);
            wc_raw(h, "</a>");
        }
        wc_raw(h, "</td><td>");
        wc_raw(h, child_type == 2ULL ? "dir" : "file");
        wc_raw(h, "</td><td align=\"right\">");
        wc_u64(h, size);
        wc_raw(h, "</td><td>");
        if (child_type == 1ULL) {
            wc_raw(h, "<a href=\"/?view=files&action=view&path=");
            (void)wc_urlenc(h, child);
            wc_raw(h, "\">view</a> <a href=\"/?view=files&action=download&path=");
            (void)wc_urlenc(h, child);
            wc_raw(h, "\">download</a> ");
        }
        wc_raw(h, "<a href=\"/?view=files&action=delete&path=");
        (void)wc_urlenc(h, child);
        wc_raw(h, "\">delete</a></td></tr>");
    }
    wc_raw(h, "</table>");
}

static int wc_pkg_iter(const pkg_installed_record *record, void *ctx_ptr) {
    wc_pkg_ctx *ctx = (wc_pkg_ctx *)ctx_ptr;
    wc_html *h;

    if (record == (const pkg_installed_record *)0 || ctx == (wc_pkg_ctx *)0) {
        return 0;
    }
    h = ctx->html;
    wc_raw(h, "<tr><td><strong>");
    wc_esc(h, record->name);
    wc_raw(h, "</strong></td><td>");
    wc_esc(h, record->version);
    wc_raw(h, "</td><td>");
    wc_esc(h, record->target);
    wc_raw(h, "</td><td><a href=\"/?view=pkg&action=remove&name=");
    (void)wc_urlenc(h, record->name);
    wc_raw(h, "\">remove</a> <a href=\"/?view=pkg&action=upgrade&name=");
    (void)wc_urlenc(h, record->name);
    wc_raw(h, "\">upgrade</a></td></tr>");
    return 1;
}

static void wc_render_pkg(wc_html *h, const wc_query *query) {
    u64 count = 0ULL;
    wc_pkg_ctx ctx;
    ush_state sh;

    ush_init_state(&sh);
    wc_section(h, "Package Manager");
    if (query->action[0] != '\0') {
        int ok = 0;
        if (strcmp(query->action, "install") == 0 && query->name[0] != '\0') {
            ok = pkg_cmd_install(&sh, query->name);
        } else if (strcmp(query->action, "remove") == 0 && query->name[0] != '\0') {
            ok = pkg_cmd_remove(query->name);
        } else if (strcmp(query->action, "update") == 0) {
            ok = pkg_cmd_update();
        } else if (strcmp(query->action, "upgrade") == 0) {
            ok = pkg_cmd_upgrade(&sh, query->name[0] != '\0' ? query->name : "--all");
        }
        wc_raw(h, ok != 0 ? "<p class=\"ok\">pkg action completed</p>"
                          : "<p class=\"err\">pkg action failed</p>");
    }

    wc_raw(h, "<p>Install, uninstall and update packages from the active pkg repository.</p>"
              "<form><input type=\"hidden\" name=\"view\" value=\"pkg\">"
              "<input type=\"hidden\" name=\"action\" value=\"install\">"
              "<input name=\"name\" placeholder=\"package name\"><button>Install</button></form>"
              "<p><a href=\"/?view=pkg&action=update\">update</a> "
              "<a href=\"/?view=pkg&action=upgrade\">upgrade all</a></p>");
    if (pkg_sqlite_init() == 0) {
        wc_raw(h, "<p>pkg database unavailable</p>");
        return;
    }
    wc_raw(h, "<h3>Installed</h3><table><tr>"
              "<th align=\"left\">Name</th><th align=\"left\">Version</th>"
              "<th align=\"left\">Target</th><th align=\"left\">Actions</th></tr>");
    if (pkg_sqlite_count_installed(&count) != 0 && count > 0ULL) {
        ctx.html = h;
        (void)pkg_sqlite_foreach_installed(wc_pkg_iter, &ctx);
    } else {
        wc_raw(h, "<tr><td colspan=\"4\">no packages installed</td></tr>");
    }
    wc_raw(h, "</table>");
}

static int wc_render_page(const char *req_path) {
    static char html_buf[WEBCONSOLE_HTML_MAX];
    wc_query query;
    wc_html h;

    wc_parse_query(req_path, &query);
    if (strcmp(query.view, "files") == 0 && strcmp(query.action, "download") == 0) {
        wc_send_file_download(query.path);
        return 1;
    }

    wc_init(&h, html_buf, (u64)sizeof(html_buf));
    wc_begin(&h, "CLeonOS Web Console");
    wc_nav(&h);
    wc_raw(&h, "<p>Local control panel for status, files, logs, network and pkg.</p>");
    if (strcmp(query.view, "files") == 0) {
        wc_render_files(&h, &query);
    } else if (strcmp(query.view, "logs") == 0) {
        wc_render_logs(&h, &query);
    } else if (strcmp(query.view, "net") == 0) {
        wc_render_net(&h);
    } else if (strcmp(query.view, "pkg") == 0) {
        wc_render_pkg(&h, &query);
    } else {
        wc_render_status(&h);
    }
    wc_end(&h);
    return wc_send_response(200, "OK", "text/html; charset=utf-8", html_buf, (u64)strlen(html_buf));
}

static int wc_parse_request_path(char *req, char *out_path, u64 out_size) {
    char *space;
    char *end;
    u64 len;

    if (req == (char *)0 || out_path == (char *)0 || out_size == 0ULL || wc_starts_with(req, "GET ") == 0) {
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

static int wc_handle_client(void) {
    char req_buf[WEBCONSOLE_REQ_MAX + 1U];
    char url_path[USH_PATH_MAX];
    u64 total = 0ULL;

    for (;;) {
        cleonos_net_tcp_recv_req req;
        u64 got;

        if (total >= WEBCONSOLE_REQ_MAX) {
            return wc_send_text(413, "Payload Too Large", "request too large\n");
        }
        req.out_payload_ptr = (u64)(usize)(req_buf + total);
        req.payload_capacity = (u64)WEBCONSOLE_REQ_MAX - total;
        req.poll_budget = WEBCONSOLE_POLL_BUDGET / 8ULL;
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
    if (wc_parse_request_path(req_buf, url_path, (u64)sizeof(url_path)) == 0 || url_path[0] != '/') {
        return wc_send_text(405, "Method Not Allowed", "only GET is supported\n");
    }
    return wc_render_page(url_path);
}

static u64 wc_parse_port(const char *text) {
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
    u64 port = WEBCONSOLE_DEFAULT_PORT;
    u64 max_requests = 0ULL;
    u64 served = 0ULL;
    int i;

    (void)envp;
    for (i = 1; i < argc; i++) {
        if (argv[i] == (char *)0) {
            continue;
        }
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = wc_parse_port(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_requests = wc_parse_port(argv[++i]);
        } else if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0) {
            puts("usage: webconsole [-p port] [-n max_requests]");
            puts("default: webconsole -p 8080");
            return 0;
        } else {
            puts("webconsole: invalid argument");
            puts("usage: webconsole [-p port] [-n max_requests]");
            return 1;
        }
    }
    if (port == 0ULL) {
        puts("webconsole: invalid port");
        return 1;
    }
    if (cleonos_sys_net_available() == 0ULL) {
        puts("webconsole: network unavailable");
        return 1;
    }

    printf("webconsole: listening on port %llu\n", (unsigned long long)port);
    for (;;) {
        cleonos_net_tcp_listen_req listen_req;
        cleonos_net_tcp_accept_req accept_req;

        listen_req.port = port;
        if (cleonos_sys_net_tcp_listen(&listen_req) == 0ULL) {
            puts("webconsole: listen failed");
            return 1;
        }
        accept_req.poll_budget = WEBCONSOLE_POLL_BUDGET;
        if (cleonos_sys_net_tcp_accept(&accept_req) == 0ULL) {
            continue;
        }
        (void)wc_handle_client();
        (void)cleonos_sys_net_tcp_close(WEBCONSOLE_POLL_BUDGET);
        served++;
        if (max_requests != 0ULL && served >= max_requests) {
            break;
        }
    }
    return 0;
}
