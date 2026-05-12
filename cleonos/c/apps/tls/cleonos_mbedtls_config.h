#ifndef CLEONOS_MBEDTLS_CONFIG_H
#define CLEONOS_MBEDTLS_CONFIG_H

#include <stdio.h>

#define MBEDTLS_CONFIG_VERSION 0x03000000

/* CLeonOS userland does not have POSIX sockets, CA storage or wall-clock time yet. */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO snprintf
#define MBEDTLS_PLATFORM_STD_MEM_HDR <stdlib.h>
#define MBEDTLS_PLATFORM_VSNPRINTF_MACRO vsnprintf
#define MBEDTLS_PLATFORM_ZEROIZE_ALT

/*
 * Verification is intentionally disabled until CLeonOS has CA storage and
 * trusted wall-clock time. Some real-world certificates contain extensions that
 * this tiny TLS profile cannot fully parse, so let the handshake keep the
 * public key/signature data and skip extension parsing failures for now.
 */
#define CLEONOS_MBEDTLS_RELAXED_X509_EXTENSIONS

#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ENTROPY_FORCE_SHA256
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_ERROR_C

#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED

#define MBEDTLS_SSL_CIPHERSUITES                                                                                       \
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,                            \
        MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, MBEDTLS_TLS_RSA_WITH_AES_256_GCM_SHA384

#define MBEDTLS_CTR_DRBG_ENTROPY_LEN 32
#define MBEDTLS_ENTROPY_MAX_SOURCES 1
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0
#define MBEDTLS_ECP_WINDOW_SIZE 2
#define MBEDTLS_MPI_MAX_SIZE 512
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

#define MBEDTLS_TEST_SW_INET_PTON

#endif
