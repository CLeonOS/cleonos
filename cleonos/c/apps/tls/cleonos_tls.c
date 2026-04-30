#include "cleonos_tls.h"

#include <mbedtls/error.h>
#include <mbedtls/memory_buffer_alloc.h>

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define CLEONOS_TLS_HEAP_SIZE (1024U * 1024U)
#define CLEONOS_TLS_IO_POLL_BUDGET 100000ULL
#define CLEONOS_TLS_HANDSHAKE_LOOPS 12000ULL

typedef unsigned char u8;

static u8 cleonos_tls_heap[CLEONOS_TLS_HEAP_SIZE];
static int cleonos_tls_heap_ready = 0;

static void cleonos_tls_contexts_init(cleonos_tls_conn *conn) {
    mbedtls_ssl_init(&conn->ssl);
    mbedtls_ssl_config_init(&conn->conf);
    mbedtls_ctr_drbg_init(&conn->ctr_drbg);
    mbedtls_entropy_init(&conn->entropy);
}

static void cleonos_tls_contexts_free(cleonos_tls_conn *conn) {
    mbedtls_ssl_free(&conn->ssl);
    mbedtls_ssl_config_free(&conn->conf);
    mbedtls_ctr_drbg_free(&conn->ctr_drbg);
    mbedtls_entropy_free(&conn->entropy);
}

void cleonos_tls_init(cleonos_tls_conn *conn) {
    if (conn == (cleonos_tls_conn *)0) {
        return;
    }

    (void)memset(conn, 0, sizeof(*conn));
    cleonos_tls_contexts_init(conn);
    conn->send_poll_budget = 200000000ULL;
    conn->recv_poll_budget = CLEONOS_TLS_IO_POLL_BUDGET;
}

static int cleonos_tls_bio_send(void *ctx, const unsigned char *buf, size_t len) {
    cleonos_tls_conn *conn = (cleonos_tls_conn *)ctx;
    cleonos_net_tcp_send_req req;
    u64 sent;

    if (conn == (cleonos_tls_conn *)0 || buf == (const unsigned char *)0 || len == 0U) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    if (len > (size_t)INT_MAX) {
        len = (size_t)INT_MAX;
    }

    req.payload_ptr = (u64)(usize)buf;
    req.payload_len = (u64)len;
    req.poll_budget = conn->send_poll_budget;
    sent = cleonos_sys_net_tcp_send(&req);
    if (sent == 0ULL || sent == (u64)-1) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    if (sent > (u64)INT_MAX) {
        sent = (u64)INT_MAX;
    }
    return (int)sent;
}

static int cleonos_tls_bio_recv(void *ctx, unsigned char *buf, size_t len) {
    cleonos_tls_conn *conn = (cleonos_tls_conn *)ctx;
    cleonos_net_tcp_recv_req req;
    u64 got;

    if (conn == (cleonos_tls_conn *)0 || buf == (unsigned char *)0 || len == 0U) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    if (len > (size_t)INT_MAX) {
        len = (size_t)INT_MAX;
    }

    req.out_payload_ptr = (u64)(usize)buf;
    req.payload_capacity = (u64)len;
    req.poll_budget = conn->recv_poll_budget;
    got = cleonos_sys_net_tcp_recv(&req);
    if (got == 0ULL) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    if (got == (u64)-1) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    if (got > (u64)INT_MAX) {
        got = (u64)INT_MAX;
    }
    return (int)got;
}

static int cleonos_tls_seed(cleonos_tls_conn *conn) {
    static const unsigned char pers[] = "cleonos-tls";

    return mbedtls_ctr_drbg_seed(&conn->ctr_drbg, mbedtls_entropy_func, &conn->entropy, pers,
                                 sizeof(pers) - 1U);
}

static void cleonos_tls_heap_init(void) {
    if (cleonos_tls_heap_ready == 0) {
        mbedtls_memory_buffer_alloc_init(cleonos_tls_heap, sizeof(cleonos_tls_heap));
        cleonos_tls_heap_ready = 1;
    }
}

static void cleonos_tls_abort_connect(cleonos_tls_conn *conn, u64 poll_budget, int error_code) {
    conn->last_error = error_code;
    (void)cleonos_sys_net_tcp_close(poll_budget);
    cleonos_tls_contexts_free(conn);
}

int cleonos_tls_connect(cleonos_tls_conn *conn, u64 ipv4_be, cleonos_tls_u16 port, const char *host,
                        u64 poll_budget) {
    cleonos_net_tcp_connect_req tcp_req;
    int ret;
    u64 loops;

    if (conn == (cleonos_tls_conn *)0 || host == (const char *)0 || host[0] == '\0' || port == 0U) {
        return 0;
    }

    cleonos_tls_heap_init();
    cleonos_tls_init(conn);
    conn->send_poll_budget = poll_budget;
    conn->recv_poll_budget = poll_budget;

    (void)memset(&tcp_req, 0, sizeof(tcp_req));
    tcp_req.dst_ipv4_be = ipv4_be;
    tcp_req.dst_port = (u64)port;
    tcp_req.src_port = 0ULL;
    tcp_req.poll_budget = poll_budget;
    if (cleonos_sys_net_tcp_connect(&tcp_req) == 0ULL) {
        conn->last_error = -1;
        cleonos_tls_contexts_free(conn);
        return 0;
    }

    ret = cleonos_tls_seed(conn);
    if (ret != 0) {
        cleonos_tls_abort_connect(conn, poll_budget, ret);
        return 0;
    }

    ret = mbedtls_ssl_config_defaults(&conn->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        cleonos_tls_abort_connect(conn, poll_budget, ret);
        return 0;
    }

    mbedtls_ssl_conf_rng(&conn->conf, mbedtls_ctr_drbg_random, &conn->ctr_drbg);
    mbedtls_ssl_conf_authmode(&conn->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_min_tls_version(&conn->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&conn->conf, MBEDTLS_SSL_VERSION_TLS1_2);

    ret = mbedtls_ssl_setup(&conn->ssl, &conn->conf);
    if (ret != 0) {
        cleonos_tls_abort_connect(conn, poll_budget, ret);
        return 0;
    }

    ret = mbedtls_ssl_set_hostname(&conn->ssl, host);
    if (ret != 0) {
        cleonos_tls_abort_connect(conn, poll_budget, ret);
        return 0;
    }

    mbedtls_ssl_set_bio(&conn->ssl, conn, cleonos_tls_bio_send, cleonos_tls_bio_recv, (void *)0);

    for (loops = 0ULL; loops < CLEONOS_TLS_HANDSHAKE_LOOPS; loops++) {
        ret = mbedtls_ssl_handshake(&conn->ssl);
        if (ret == 0) {
            conn->active = 1;
            conn->last_error = 0;
            return 1;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            (void)cleonos_sys_sleep_ticks(1ULL);
            continue;
        }

        cleonos_tls_abort_connect(conn, poll_budget, ret);
        return 0;
    }

    cleonos_tls_abort_connect(conn, poll_budget, MBEDTLS_ERR_SSL_TIMEOUT);
    return 0;
}

int cleonos_tls_write_all(cleonos_tls_conn *conn, const void *buffer, u64 length) {
    const u8 *bytes = (const u8 *)buffer;
    u64 done = 0ULL;

    if (conn == (cleonos_tls_conn *)0 || buffer == (const void *)0) {
        return 0;
    }

    while (done < length) {
        size_t chunk = (size_t)(length - done);
        int ret;

        if (chunk > (size_t)INT_MAX) {
            chunk = (size_t)INT_MAX;
        }

        ret = mbedtls_ssl_write(&conn->ssl, bytes + done, chunk);
        if (ret > 0) {
            done += (u64)ret;
            continue;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            (void)cleonos_sys_sleep_ticks(1ULL);
            continue;
        }

        conn->last_error = ret;
        return 0;
    }

    conn->last_error = 0;
    return 1;
}

int cleonos_tls_read(cleonos_tls_conn *conn, void *buffer, u64 capacity) {
    int ret;

    if (conn == (cleonos_tls_conn *)0 || buffer == (void *)0 || capacity == 0ULL) {
        return -1;
    }
    if (capacity > (u64)INT_MAX) {
        capacity = (u64)INT_MAX;
    }

    ret = mbedtls_ssl_read(&conn->ssl, (unsigned char *)buffer, (size_t)capacity);
    if (ret > 0) {
        conn->last_error = 0;
        return ret;
    }
    if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        conn->eof = 1;
        conn->last_error = 0;
        return 0;
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        conn->last_error = 0;
        return 0;
    }

    conn->last_error = ret;
    return -1;
}

int cleonos_tls_eof(const cleonos_tls_conn *conn) {
    if (conn == (const cleonos_tls_conn *)0) {
        return 1;
    }
    return conn->eof;
}

void cleonos_tls_close(cleonos_tls_conn *conn, u64 poll_budget) {
    u64 loops;

    if (conn == (cleonos_tls_conn *)0) {
        return;
    }

    if (conn->active != 0) {
        for (loops = 0ULL; loops < 8ULL; loops++) {
            int ret = mbedtls_ssl_close_notify(&conn->ssl);
            if (ret == 0) {
                break;
            }
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                break;
            }
            (void)cleonos_sys_sleep_ticks(1ULL);
        }
    }

    (void)cleonos_sys_net_tcp_close(poll_budget);
    cleonos_tls_contexts_free(conn);
    (void)memset(conn, 0, sizeof(*conn));
}

int cleonos_tls_last_error(const cleonos_tls_conn *conn) {
    if (conn == (const cleonos_tls_conn *)0) {
        return 0;
    }
    return conn->last_error;
}

void cleonos_tls_error_text(int error_code, char *out, u64 out_size) {
    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (error_code == 0) {
        (void)strncpy(out, "ok", (size_t)out_size - 1U);
        out[out_size - 1ULL] = '\0';
        return;
    }

    mbedtls_strerror(error_code, out, (size_t)out_size);
    out[out_size - 1ULL] = '\0';
}
