#ifndef CLEONOS_TLS_H
#define CLEONOS_TLS_H

#include <cleonos_syscall.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>

typedef unsigned short cleonos_tls_u16;

typedef struct cleonos_tls_conn {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    u64 send_poll_budget;
    u64 recv_poll_budget;
    int active;
    int last_error;
    int eof;
} cleonos_tls_conn;

void cleonos_tls_init(cleonos_tls_conn *conn);
int cleonos_tls_connect(cleonos_tls_conn *conn, u64 ipv4_be, cleonos_tls_u16 port, const char *host,
                        u64 poll_budget);
int cleonos_tls_write_all(cleonos_tls_conn *conn, const void *buffer, u64 length);
int cleonos_tls_read(cleonos_tls_conn *conn, void *buffer, u64 capacity);
int cleonos_tls_eof(const cleonos_tls_conn *conn);
void cleonos_tls_close(cleonos_tls_conn *conn, u64 poll_budget);
int cleonos_tls_last_error(const cleonos_tls_conn *conn);
void cleonos_tls_error_text(int error_code, char *out, u64 out_size);

#endif
