#include <stdlib.h>

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include <cleonos_syscall.h>

static int clib_digit_value(int ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'z') {
        return 10 + (ch - 'a');
    }

    if (ch >= 'A' && ch <= 'Z') {
        return 10 + (ch - 'A');
    }

    return -1;
}

static const char *clib_skip_space(const char *text) {
    const char *p = text;

    if (p == (const char *)0) {
        return (const char *)0;
    }

    while (*p != '\0' && isspace((unsigned char)*p) != 0) {
        p++;
    }

    return p;
}

int abs(int value) {
    return (value < 0) ? -value : value;
}

long labs(long value) {
    return (value < 0L) ? -value : value;
}

long long llabs(long long value) {
    return (value < 0LL) ? -value : value;
}

double fabs(double value) {
    return (value < 0.0) ? -value : value;
}

float fabsf(float value) {
    return (value < 0.0f) ? -value : value;
}

long double fabsl(long double value) {
    return (value < 0.0L) ? -value : value;
}

int atoi(const char *text) {
    return (int)strtol(text, (char **)0, 10);
}

long atol(const char *text) {
    return strtol(text, (char **)0, 10);
}

long long atoll(const char *text) {
    return strtoll(text, (char **)0, 10);
}

unsigned long strtoul(const char *text, char **out_end, int base) {
    const char *p = clib_skip_space(text);
    int negative = 0;
    unsigned long value = 0UL;
    int any = 0;
    int overflow = 0;

    if (out_end != (char **)0) {
        *out_end = (char *)text;
    }

    if (p == (const char *)0) {
        return 0UL;
    }

    if (*p == '+' || *p == '-') {
        negative = (*p == '-') ? 1 : 0;
        p++;
    }

    if (base == 0) {
        if (p[0] == '0') {
            if ((p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2]) != 0) {
                base = 16;
                p += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }

    if (base < 2 || base > 36) {
        return 0UL;
    }

    while (*p != '\0') {
        int digit = clib_digit_value((unsigned char)*p);

        if (digit < 0 || digit >= base) {
            break;
        }

        any = 1;

        if (value > (ULONG_MAX - (unsigned long)digit) / (unsigned long)base) {
            overflow = 1;
            value = ULONG_MAX;
        } else if (overflow == 0) {
            value = value * (unsigned long)base + (unsigned long)digit;
        }

        p++;
    }

    if (any == 0) {
        return 0UL;
    }

    if (out_end != (char **)0) {
        *out_end = (char *)p;
    }

    if (negative != 0) {
        return (unsigned long)(0UL - value);
    }

    return value;
}

long strtol(const char *text, char **out_end, int base) {
    const char *p = clib_skip_space(text);
    int negative = 0;
    unsigned long long value = 0ULL;
    unsigned long long limit;
    int any = 0;
    int overflow = 0;

    if (out_end != (char **)0) {
        *out_end = (char *)text;
    }

    if (p == (const char *)0) {
        return 0L;
    }

    if (*p == '+' || *p == '-') {
        negative = (*p == '-') ? 1 : 0;
        p++;
    }

    if (base == 0) {
        if (p[0] == '0') {
            if ((p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2]) != 0) {
                base = 16;
                p += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }

    if (base < 2 || base > 36) {
        return 0L;
    }

    limit = (negative != 0) ? ((unsigned long long)LONG_MAX + 1ULL) : (unsigned long long)LONG_MAX;

    while (*p != '\0') {
        int digit = clib_digit_value((unsigned char)*p);

        if (digit < 0 || digit >= base) {
            break;
        }

        any = 1;

        if (value > (limit - (unsigned long long)digit) / (unsigned long long)base) {
            overflow = 1;
            value = limit;
        } else if (overflow == 0) {
            value = value * (unsigned long long)base + (unsigned long long)digit;
        }

        p++;
    }

    if (any == 0) {
        return 0L;
    }

    if (out_end != (char **)0) {
        *out_end = (char *)p;
    }

    if (overflow != 0) {
        return (negative != 0) ? LONG_MIN : LONG_MAX;
    }

    if (negative != 0) {
        if (value == ((unsigned long long)LONG_MAX + 1ULL)) {
            return LONG_MIN;
        }
        return -(long)value;
    }

    return (long)value;
}

long long strtoll(const char *text, char **out_end, int base) {
    return (long long)strtol(text, out_end, base);
}

unsigned long long strtoull(const char *text, char **out_end, int base) {
    return (unsigned long long)strtoul(text, out_end, base);
}

#define CLIB_HEAP_CAPACITY (4U * 1024U * 1024U)
#define CLIB_HEAP_ALIGN 8U

typedef union clib_heap_storage {
    unsigned long long align;
    unsigned char bytes[CLIB_HEAP_CAPACITY];
} clib_heap_storage;

static clib_heap_storage clib_heap;
static size_t clib_heap_used = 0U;

static size_t clib_align_up(size_t value, size_t align) {
    size_t mask;

    if (align == 0U) {
        return value;
    }

    mask = align - 1U;
    return (value + mask) & ~mask;
}

__attribute__((weak)) void *malloc(size_t size) {
    size_t need;
    size_t begin;
    size_t end;
    size_t *hdr;

    if (size == 0U) {
        return (void *)0;
    }

    size = clib_align_up(size, CLIB_HEAP_ALIGN);
    need = sizeof(size_t) + size;
    begin = clib_align_up(clib_heap_used, CLIB_HEAP_ALIGN);

    if (begin > (size_t)CLIB_HEAP_CAPACITY || need > (size_t)CLIB_HEAP_CAPACITY - begin) {
        return (void *)0;
    }

    end = begin + need;
    hdr = (size_t *)(void *)(clib_heap.bytes + begin);
    *hdr = size;
    clib_heap_used = end;
    return (void *)(hdr + 1);
}

__attribute__((weak)) void free(void *ptr) {
    (void)ptr;
    /* monotonic allocator: memory is reclaimed when process exits */
}

__attribute__((weak)) void *calloc(size_t count, size_t size) {
    size_t total;
    void *ptr;

    if (count == 0U || size == 0U) {
        return (void *)0;
    }

    if (count > ((size_t)-1) / size) {
        return (void *)0;
    }

    total = count * size;
    ptr = malloc(total);
    if (ptr == (void *)0) {
        return (void *)0;
    }

    (void)memset(ptr, 0, total);
    return ptr;
}

__attribute__((weak)) void *realloc(void *ptr, size_t size) {
    void *out;
    size_t old_size;

    if (ptr == (void *)0) {
        return malloc(size);
    }

    if (size == 0U) {
        free(ptr);
        return (void *)0;
    }

    old_size = *((size_t *)ptr - 1U);
    out = malloc(size);
    if (out == (void *)0) {
        return (void *)0;
    }

    if (old_size < size) {
        (void)memcpy(out, ptr, old_size);
    } else {
        (void)memcpy(out, ptr, size);
    }

    free(ptr);
    return out;
}

static unsigned long clib_rand_state = 1UL;

void srand(unsigned int seed) {
    clib_rand_state = (unsigned long)seed;
    if (clib_rand_state == 0UL) {
        clib_rand_state = 1UL;
    }
}

int rand(void) {
    clib_rand_state = (1103515245UL * clib_rand_state) + 12345UL;
    return (int)((clib_rand_state >> 16) & (unsigned long)RAND_MAX);
}

void exit(int status) {
    (void)cleonos_sys_exit((u64)(unsigned long long)status);

    for (;;) {
        (void)cleonos_sys_yield();
    }
}

void abort(void) {
    exit(EXIT_FAILURE);
}
