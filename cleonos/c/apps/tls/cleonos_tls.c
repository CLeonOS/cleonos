#include "cleonos_tls.h"

#include <mbedtls/asn1.h>
#include <mbedtls/platform.h>
#include <mbedtls/x509.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLEONOS_TLS_IO_POLL_BUDGET 100000ULL
#define CLEONOS_TLS_HANDSHAKE_LOOPS 12000ULL
#define CLEONOS_TLS_ALLOC_MAGIC 0x544C5348U

typedef unsigned char u8;

typedef struct cleonos_tls_alloc_header {
    size_t size;
    unsigned int magic;
    unsigned int reserved;
} cleonos_tls_alloc_header;

static int cleonos_tls_allocator_ready = 0;
static size_t cleonos_tls_alloc_current = 0U;
static size_t cleonos_tls_alloc_peak = 0U;
static size_t cleonos_tls_alloc_count = 0U;
static size_t cleonos_tls_alloc_fail_count = 0U;

static u64 cleonos_tls_strlen(const char *text) {
    u64 len = 0ULL;

    if (text == (const char *)0) {
        return 0ULL;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static void cleonos_tls_append_text(char *out, u64 out_size, const char *text) {
    u64 at;
    u64 i = 0ULL;

    if (out == (char *)0 || out_size == 0ULL || text == (const char *)0) {
        return;
    }

    at = cleonos_tls_strlen(out);
    if (at >= out_size) {
        out[out_size - 1ULL] = '\0';
        return;
    }

    while (text[i] != '\0' && at + 1ULL < out_size) {
        out[at++] = text[i++];
    }
    out[at] = '\0';
}

static void cleonos_tls_log_line(const char *line) {
    if (line == (const char *)0 || line[0] == '\0') {
        return;
    }

    (void)cleonos_sys_log_write(line, cleonos_tls_strlen(line));
}

static const char *cleonos_tls_error_name(int error_code) {
    switch (error_code) {
    case -1:
        return "tcp connect failed";
    case MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE:
        return "ssl feature unavailable";
    case MBEDTLS_ERR_SSL_BAD_INPUT_DATA:
        return "ssl bad input data";
    case MBEDTLS_ERR_SSL_INVALID_MAC:
        return "ssl invalid mac";
    case MBEDTLS_ERR_SSL_INVALID_RECORD:
        return "ssl invalid record";
    case MBEDTLS_ERR_SSL_CONN_EOF:
        return "ssl connection eof";
    case MBEDTLS_ERR_SSL_DECODE_ERROR:
        return "ssl decode error";
    case MBEDTLS_ERR_SSL_NO_RNG:
        return "ssl no rng";
    case MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION:
        return "ssl unsupported extension";
    case MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE:
        return "ssl unexpected message";
    case MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE:
        return "ssl fatal alert";
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
        return "ssl peer close notify";
    case MBEDTLS_ERR_SSL_BAD_CERTIFICATE:
        return "ssl bad certificate";
    case MBEDTLS_ERR_SSL_ALLOC_FAILED:
        return "ssl allocation failed";
    case MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION:
        return "ssl bad protocol version";
    case MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE:
        return "ssl handshake failure";
    case MBEDTLS_ERR_SSL_INTERNAL_ERROR:
        return "ssl internal error";
    case MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL:
        return "ssl buffer too small";
    case MBEDTLS_ERR_SSL_WANT_READ:
        return "ssl want read";
    case MBEDTLS_ERR_SSL_WANT_WRITE:
        return "ssl want write";
    case MBEDTLS_ERR_SSL_TIMEOUT:
        return "ssl timeout";
    case MBEDTLS_ERR_SSL_UNEXPECTED_RECORD:
        return "ssl unexpected record";
    case MBEDTLS_ERR_SSL_BAD_CONFIG:
        return "ssl bad config";
    case MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE:
        return "x509 feature unavailable";
    case MBEDTLS_ERR_X509_UNKNOWN_OID:
        return "x509 unknown oid";
    case MBEDTLS_ERR_X509_INVALID_FORMAT:
        return "x509 invalid format";
    case MBEDTLS_ERR_X509_INVALID_VERSION:
        return "x509 invalid version";
    case MBEDTLS_ERR_X509_INVALID_SERIAL:
        return "x509 invalid serial";
    case MBEDTLS_ERR_X509_INVALID_ALG:
        return "x509 invalid algorithm";
    case MBEDTLS_ERR_X509_INVALID_NAME:
        return "x509 invalid name";
    case MBEDTLS_ERR_X509_INVALID_DATE:
        return "x509 invalid date";
    case MBEDTLS_ERR_X509_INVALID_SIGNATURE:
        return "x509 invalid signature";
    case MBEDTLS_ERR_X509_INVALID_EXTENSIONS:
        return "x509 invalid extensions";
    case MBEDTLS_ERR_X509_UNKNOWN_VERSION:
        return "x509 unknown version";
    case MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG:
        return "x509 unknown signature algorithm";
    case MBEDTLS_ERR_X509_SIG_MISMATCH:
        return "x509 signature mismatch";
    case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
        return "x509 certificate verify failed";
    case MBEDTLS_ERR_X509_BAD_INPUT_DATA:
        return "x509 bad input data";
    case MBEDTLS_ERR_X509_ALLOC_FAILED:
        return "x509 allocation failed";
    case MBEDTLS_ERR_X509_BUFFER_TOO_SMALL:
        return "x509 buffer too small";
    case MBEDTLS_ERR_ASN1_OUT_OF_DATA:
        return "asn1 out of data";
    case MBEDTLS_ERR_ASN1_UNEXPECTED_TAG:
        return "asn1 unexpected tag";
    case MBEDTLS_ERR_ASN1_INVALID_LENGTH:
        return "asn1 invalid length";
    case MBEDTLS_ERR_ASN1_LENGTH_MISMATCH:
        return "asn1 length mismatch";
    case MBEDTLS_ERR_ASN1_INVALID_DATA:
        return "asn1 invalid data";
    case MBEDTLS_ERR_ASN1_ALLOC_FAILED:
        return "asn1 allocation failed";
    case MBEDTLS_ERR_ASN1_BUF_TOO_SMALL:
        return "asn1 buffer too small";
    case MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED:
        return "ctr_drbg entropy source failed";
    case MBEDTLS_ERR_ENTROPY_SOURCE_FAILED:
        return "entropy source failed";
    case MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE:
        return "entropy no strong source";
    default:
        return (const char *)0;
    }
}

static void cleonos_tls_log_error(const char *stage, int error_code) {
    char text[160];
    char error_text[96];

    cleonos_tls_error_text(error_code, error_text, (u64)sizeof(error_text));
    if (snprintf(text, sizeof(text), "[TLS] %s: %s (code=%d)", (stage == (const char *)0) ? "error" : stage, error_text,
                 error_code) <= 0) {
        cleonos_tls_log_line("[TLS] error formatting failed");
        return;
    }

    cleonos_tls_log_line(text);
}

static void cleonos_tls_log_u64(const char *name, u64 value) {
    char text[96];

    if (name == (const char *)0) {
        name = "VALUE";
    }
    if (snprintf(text, sizeof(text), "[TLS] %s: 0x%llX", name, (unsigned long long)value) <= 0) {
        cleonos_tls_log_line("[TLS] hex log formatting failed");
        return;
    }

    cleonos_tls_log_line(text);
}

static void cleonos_tls_log_heap_stats(const char *stage) {
    char text[160];

    if (snprintf(text, sizeof(text), "[TLS] %s heap cur=%llu peak=%llu alloc=%llu fail=%llu",
                 (stage == (const char *)0) ? "stats" : stage, (unsigned long long)cleonos_tls_alloc_current,
                 (unsigned long long)cleonos_tls_alloc_peak, (unsigned long long)cleonos_tls_alloc_count,
                 (unsigned long long)cleonos_tls_alloc_fail_count) <= 0) {
        cleonos_tls_log_line("[TLS] heap stats formatting failed");
        return;
    }

    cleonos_tls_log_line(text);
}

static void cleonos_tls_contexts_init(cleonos_tls_conn *conn) {
    if (conn == (cleonos_tls_conn *)0) {
        return;
    }

    mbedtls_ssl_init(&conn->ssl);
    mbedtls_ssl_config_init(&conn->conf);
    mbedtls_ctr_drbg_init(&conn->ctr_drbg);
    mbedtls_entropy_init(&conn->entropy);
    conn->initialized = 1;
}

static void cleonos_tls_contexts_free(cleonos_tls_conn *conn) {
    if (conn == (cleonos_tls_conn *)0 || conn->initialized == 0) {
        return;
    }

    mbedtls_ssl_free(&conn->ssl);
    mbedtls_ssl_config_free(&conn->conf);
    mbedtls_ctr_drbg_free(&conn->ctr_drbg);
    mbedtls_entropy_free(&conn->entropy);
    conn->initialized = 0;
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

    return mbedtls_ctr_drbg_seed(&conn->ctr_drbg, mbedtls_entropy_func, &conn->entropy, pers, sizeof(pers) - 1U);
}

static void *cleonos_tls_calloc(size_t count, size_t size) {
    cleonos_tls_alloc_header *hdr;
    size_t payload_size;
    size_t total_size;

    if (count == 0U || size == 0U) {
        return (void *)0;
    }
    if (count > ((size_t)-1) / size) {
        cleonos_tls_alloc_fail_count++;
        return (void *)0;
    }

    payload_size = count * size;
    if (payload_size > ((size_t)-1) - sizeof(*hdr)) {
        cleonos_tls_alloc_fail_count++;
        return (void *)0;
    }

    total_size = sizeof(*hdr) + payload_size;
    hdr = (cleonos_tls_alloc_header *)malloc(total_size);
    if (hdr == (cleonos_tls_alloc_header *)0) {
        cleonos_tls_alloc_fail_count++;
        cleonos_tls_log_u64("ALLOC_FAILED_BYTES", (u64)payload_size);
        return (void *)0;
    }

    hdr->size = payload_size;
    hdr->magic = CLEONOS_TLS_ALLOC_MAGIC;
    hdr->reserved = 0U;
    (void)memset((void *)(hdr + 1), 0, payload_size);

    cleonos_tls_alloc_current += payload_size;
    if (cleonos_tls_alloc_current > cleonos_tls_alloc_peak) {
        cleonos_tls_alloc_peak = cleonos_tls_alloc_current;
    }
    cleonos_tls_alloc_count++;
    return (void *)(hdr + 1);
}

static void cleonos_tls_free(void *ptr) {
    cleonos_tls_alloc_header *hdr;

    if (ptr == (void *)0) {
        return;
    }

    hdr = ((cleonos_tls_alloc_header *)ptr) - 1;
    if (hdr->magic != CLEONOS_TLS_ALLOC_MAGIC) {
        cleonos_tls_log_line("[TLS] free rejected invalid block");
        return;
    }

    if (cleonos_tls_alloc_current >= hdr->size) {
        cleonos_tls_alloc_current -= hdr->size;
    } else {
        cleonos_tls_alloc_current = 0U;
    }

    (void)memset(ptr, 0, hdr->size);
    hdr->magic = 0U;
    hdr->size = 0U;
    free((void *)hdr);
}

static int cleonos_tls_heap_init(void) {
    if (cleonos_tls_allocator_ready == 0) {
        if (mbedtls_platform_set_calloc_free(cleonos_tls_calloc, cleonos_tls_free) != 0) {
            cleonos_tls_log_error("allocator init failed", MBEDTLS_ERR_SSL_ALLOC_FAILED);
            return 0;
        }
        cleonos_tls_allocator_ready = 1;
        cleonos_tls_log_line("[TLS] dynamic allocator ready");
    }

    if (cleonos_tls_alloc_current == 0U) {
        cleonos_tls_alloc_peak = 0U;
        cleonos_tls_alloc_count = 0U;
        cleonos_tls_alloc_fail_count = 0U;
    }

    return 1;
}

static void cleonos_tls_heap_reset(void) {
    cleonos_tls_log_heap_stats("after cleanup");
}

static int cleonos_tls_rng(void *ctx, unsigned char *output, size_t len) {
    if (ctx == (void *)0 || (len != 0U && output == (unsigned char *)0) || (u64)(usize)output < 0x10000ULL) {
        cleonos_tls_log_line("[TLS] RNG rejected invalid output buffer");
        cleonos_tls_log_u64("RNG_CTX", (u64)(usize)ctx);
        cleonos_tls_log_u64("RNG_OUTPUT", (u64)(usize)output);
        cleonos_tls_log_u64("RNG_LEN", (u64)len);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return mbedtls_ctr_drbg_random(ctx, output, len);
}

static void cleonos_tls_drop_context_preserve_error(cleonos_tls_conn *conn, int error_code) {
    if (conn == (cleonos_tls_conn *)0) {
        return;
    }

    cleonos_tls_contexts_free(conn);
    cleonos_tls_heap_reset();
    (void)memset(conn, 0, sizeof(*conn));
    conn->last_error = error_code;
}

static void cleonos_tls_abort_connect(cleonos_tls_conn *conn, u64 poll_budget, int error_code) {
    cleonos_tls_log_error("connect failed", error_code);
    cleonos_tls_log_heap_stats("connect failed");
    (void)cleonos_sys_net_tcp_close(poll_budget);
    cleonos_tls_drop_context_preserve_error(conn, error_code);
}

int cleonos_tls_connect_deadline(cleonos_tls_conn *conn, u64 ipv4_be, cleonos_tls_u16 port, const char *host,
                                 u64 poll_budget, u64 deadline_tick) {
    cleonos_net_tcp_connect_req tcp_req;
    int ret;
    u64 loops;

    if (conn == (cleonos_tls_conn *)0 || host == (const char *)0 || host[0] == '\0' || port == 0U) {
        return 0;
    }

    if (cleonos_tls_heap_init() == 0) {
        (void)memset(conn, 0, sizeof(*conn));
        conn->last_error = MBEDTLS_ERR_SSL_ALLOC_FAILED;
        return 0;
    }
    cleonos_tls_init(conn);
    conn->send_poll_budget = poll_budget;
    conn->recv_poll_budget = poll_budget;

    (void)memset(&tcp_req, 0, sizeof(tcp_req));
    tcp_req.dst_ipv4_be = ipv4_be;
    tcp_req.dst_port = (u64)port;
    tcp_req.src_port = 0ULL;
    tcp_req.poll_budget = poll_budget;
    if (cleonos_sys_net_tcp_connect(&tcp_req) == 0ULL) {
        cleonos_tls_log_error("tcp connect failed", -1);
        cleonos_tls_log_u64("TCP_LAST_ERROR", cleonos_sys_net_tcp_last_error());
        cleonos_tls_drop_context_preserve_error(conn, -1);
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

    mbedtls_ssl_conf_rng(&conn->conf, cleonos_tls_rng, &conn->ctr_drbg);
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
        if (deadline_tick != 0ULL && cleonos_sys_timer_ticks() >= deadline_tick) {
            break;
        }
        ret = mbedtls_ssl_handshake(&conn->ssl);
        if (ret == 0) {
            conn->active = 1;
            conn->last_error = 0;
            cleonos_tls_log_line("[TLS] handshake ok");
            cleonos_tls_log_heap_stats("handshake ok");
            return 1;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            (void)cleonos_sys_sleep_ticks(1ULL);
            continue;
        }

        cleonos_tls_log_u64("HANDSHAKE_LOOP", loops);
        cleonos_tls_abort_connect(conn, poll_budget, ret);
        return 0;
    }

    cleonos_tls_abort_connect(conn, poll_budget, MBEDTLS_ERR_SSL_TIMEOUT);
    return 0;
}

int cleonos_tls_connect(cleonos_tls_conn *conn, u64 ipv4_be, cleonos_tls_u16 port, const char *host, u64 poll_budget) {
    return cleonos_tls_connect_deadline(conn, ipv4_be, port, host, poll_budget, 0ULL);
}

int cleonos_tls_write_all(cleonos_tls_conn *conn, const void *buffer, u64 length) {
    const u8 *bytes = (const u8 *)buffer;
    u64 done = 0ULL;

    if (conn == (cleonos_tls_conn *)0 || conn->initialized == 0 || conn->active == 0 || buffer == (const void *)0) {
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
        cleonos_tls_log_error("write failed", ret);
        cleonos_tls_log_heap_stats("write failed");
        return 0;
    }

    conn->last_error = 0;
    return 1;
}

int cleonos_tls_read(cleonos_tls_conn *conn, void *buffer, u64 capacity) {
    int ret;

    if (conn == (cleonos_tls_conn *)0 || conn->initialized == 0 || conn->active == 0 || buffer == (void *)0 ||
        capacity == 0ULL) {
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
    cleonos_tls_log_error("read failed", ret);
    cleonos_tls_log_heap_stats("read failed");
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

    if (conn->initialized != 0 && conn->active != 0) {
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
    cleonos_tls_heap_reset();
    (void)memset(conn, 0, sizeof(*conn));
}

int cleonos_tls_last_error(const cleonos_tls_conn *conn) {
    if (conn == (const cleonos_tls_conn *)0) {
        return 0;
    }
    return conn->last_error;
}

void cleonos_tls_error_text(int error_code, char *out, u64 out_size) {
    int code_abs;
    int high_part;
    int low_part;
    const char *exact;
    const char *high_text;
    const char *low_text;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (error_code == 0) {
        (void)strncpy(out, "ok", (size_t)out_size - 1U);
        out[out_size - 1ULL] = '\0';
        return;
    }

    exact = cleonos_tls_error_name(error_code);
    if (exact != (const char *)0) {
        cleonos_tls_append_text(out, out_size, exact);
        return;
    }

    code_abs = (error_code < 0) ? -error_code : error_code;
    high_part = code_abs & 0xFF80;
    low_part = code_abs & ~0xFF80;

    high_text = (high_part != 0) ? cleonos_tls_error_name(-high_part) : (const char *)0;
    low_text = (low_part != 0) ? cleonos_tls_error_name(-low_part) : (const char *)0;

    if (high_text != (const char *)0) {
        cleonos_tls_append_text(out, out_size, high_text);
    }
    if (low_text != (const char *)0) {
        if (out[0] != '\0') {
            cleonos_tls_append_text(out, out_size, " : ");
        }
        cleonos_tls_append_text(out, out_size, low_text);
    }

    if (out[0] == '\0') {
        cleonos_tls_append_text(out, out_size, "mbedtls error");
    }
}
