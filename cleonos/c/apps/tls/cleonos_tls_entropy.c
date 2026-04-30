#include <stddef.h>
#include <string.h>

#include <cleonos_syscall.h>

typedef unsigned char u8;
typedef unsigned long long u64_local;

static u64_local cleonos_tls_entropy_state = 0x434C4B53544C5331ULL;

static u64_local cleonos_tls_mix64(u64_local x) {
    x ^= x >> 30U;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27U;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31U;
    return x;
}

static int cleonos_tls_cpu_has_rdrand(void) {
#if defined(__x86_64__) || defined(__i386__)
    static int cached = -1;
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    if (cached >= 0) {
        return cached;
    }

    eax = 1U;
    ebx = 0U;
    ecx = 0U;
    edx = 0U;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ebx;
    (void)edx;
    cached = ((ecx & (1U << 30U)) != 0U) ? 1 : 0;
    return cached;
#else
    return 0;
#endif
}

static int cleonos_tls_rdrand64(u64_local *out) {
#if defined(__x86_64__) || defined(__i386__)
    unsigned char ok;
    u64_local value;

    if (cleonos_tls_cpu_has_rdrand() == 0) {
        return 0;
    }

#if defined(__x86_64__)
    __asm__ volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(ok));
#else
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(lo), "=qm"(ok));
    __asm__ volatile("rdrand %0" : "=r"(hi));
    value = ((u64_local)hi << 32U) | (u64_local)lo;
#endif
    if (ok != 0U) {
        *out = value;
        return 1;
    }
#else
    (void)out;
#endif
    return 0;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    size_t i = 0U;
    u64_local seed;

    (void)data;
    if (output == (unsigned char *)0 || olen == (size_t *)0) {
        return -1;
    }

    seed = (u64_local)cleonos_sys_timer_ticks() ^ (u64_local)(usize)&seed ^ cleonos_tls_entropy_state;
    cleonos_tls_entropy_state = cleonos_tls_mix64(seed + 0x9E3779B97F4A7C15ULL);

    while (i < len) {
        u64_local value;
        size_t j;

        if (cleonos_tls_rdrand64(&value) == 0) {
            cleonos_tls_entropy_state = cleonos_tls_mix64(cleonos_tls_entropy_state + (u64_local)i +
                                                          (u64_local)cleonos_sys_timer_ticks());
            value = cleonos_tls_entropy_state;
        }

        for (j = 0U; j < sizeof(value) && i < len; j++) {
            output[i++] = (u8)((value >> (j * 8U)) & 0xFFU);
        }
    }

    *olen = len;
    return 0;
}

void mbedtls_platform_zeroize(void *buf, size_t len) {
    volatile u8 *p = (volatile u8 *)buf;

    if (p == (volatile u8 *)0) {
        return;
    }

    while (len > 0U) {
        *p = 0U;
        p++;
        len--;
    }
}
