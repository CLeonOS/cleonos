#include "browser_internal.h"

#include <stdio.h>
#include <string.h>

#include "../tls/cleonos_tls.h"

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
