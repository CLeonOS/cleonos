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

double strtod(const char *text, char **out_end) {
    const char *p = clib_skip_space(text);
    double value = 0.0;
    double scale = 1.0;
    int negative = 0;
    int any = 0;
    int exp_negative = 0;
    int exp_value = 0;

    if (out_end != (char **)0) {
        *out_end = (char *)text;
    }

    if (p == (const char *)0) {
        return 0.0;
    }

    if (*p == '+' || *p == '-') {
        negative = (*p == '-') ? 1 : 0;
        p++;
    }

    while (isdigit((unsigned char)*p) != 0) {
        value = (value * 10.0) + (double)(*p - '0');
        any = 1;
        p++;
    }

    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p) != 0) {
            scale *= 0.1;
            value += (double)(*p - '0') * scale;
            any = 1;
            p++;
        }
    }

    if (any != 0 && (*p == 'e' || *p == 'E')) {
        const char *exp_start = p;

        p++;
        if (*p == '+' || *p == '-') {
            exp_negative = (*p == '-') ? 1 : 0;
            p++;
        }

        if (isdigit((unsigned char)*p) == 0) {
            p = exp_start;
        } else {
            while (isdigit((unsigned char)*p) != 0) {
                if (exp_value < 308) {
                    exp_value = (exp_value * 10) + (*p - '0');
                }
                p++;
            }

            while (exp_value > 0) {
                value = (exp_negative != 0) ? (value * 0.1) : (value * 10.0);
                exp_value--;
            }
        }
    }

    if (any == 0) {
        return 0.0;
    }

    if (out_end != (char **)0) {
        *out_end = (char *)p;
    }

    return (negative != 0) ? -value : value;
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

#ifndef CLIB_HEAP_CAPACITY
#define CLIB_HEAP_CAPACITY (256U * 1024U)
#endif
#define CLIB_HEAP_ALIGN 16U
#define CLIB_HEAP_MAGIC 0xC10B5A7EU
#define CLIB_HEAP_CHUNK_MIN (64U * 1024U)
#define CLIB_HEAP_CHUNK_MAX (4U * 1024U * 1024U)

typedef struct clib_heap_block {
    size_t size;
    unsigned int used;
    unsigned int magic;
    struct clib_heap_block *next;
    struct clib_heap_block *prev;
} clib_heap_block;

static clib_heap_block *clib_heap_head = (clib_heap_block *)0;
static clib_heap_block *clib_heap_tail = (clib_heap_block *)0;

static size_t clib_align_up(size_t value, size_t align) {
    size_t mask;

    if (align == 0U) {
        return value;
    }

    mask = align - 1U;
    return (value + mask) & ~mask;
}

static size_t clib_heap_header_size(void) {
    return clib_align_up(sizeof(clib_heap_block), CLIB_HEAP_ALIGN);
}

static void clib_heap_split(clib_heap_block *block, size_t need) {
    size_t header_size = clib_heap_header_size();
    unsigned char *next_addr;
    clib_heap_block *next;

    if (block == (clib_heap_block *)0 || block->size <= need + header_size + CLIB_HEAP_ALIGN) {
        return;
    }

    next_addr = ((unsigned char *)(void *)block) + header_size + need;
    next = (clib_heap_block *)(void *)next_addr;
    next->size = block->size - need - header_size;
    next->used = 0U;
    next->magic = CLIB_HEAP_MAGIC;
    next->next = block->next;
    next->prev = block;
    if (next->next != (clib_heap_block *)0) {
        next->next->prev = next;
    } else {
        clib_heap_tail = next;
    }

    block->next = next;
    block->size = need;
}

static void clib_heap_merge_next(clib_heap_block *block) {
    size_t header_size = clib_heap_header_size();
    clib_heap_block *next;

    if (block == (clib_heap_block *)0 || block->used != 0U) {
        return;
    }

    next = block->next;
    if (next == (clib_heap_block *)0 || next->magic != CLIB_HEAP_MAGIC || next->used != 0U) {
        return;
    }

    block->size += header_size + next->size;
    block->next = next->next;
    if (block->next != (clib_heap_block *)0) {
        block->next->prev = block;
    } else {
        clib_heap_tail = block;
    }
}

static clib_heap_block *clib_heap_block_from_ptr(void *ptr) {
    size_t header_size = clib_heap_header_size();
    clib_heap_block *current;

    if (ptr == (void *)0) {
        return (clib_heap_block *)0;
    }

    current = clib_heap_head;
    while (current != (clib_heap_block *)0) {
        void *payload = (void *)(((unsigned char *)(void *)current) + header_size);

        if (current->magic != CLIB_HEAP_MAGIC) {
            return (clib_heap_block *)0;
        }

        if (payload == ptr && current->used != 0U) {
            return current;
        }

        current = current->next;
    }

    return (clib_heap_block *)0;
}

static clib_heap_block *clib_heap_request_chunk(size_t need) {
    size_t header_size = clib_heap_header_size();
    size_t chunk_size = (size_t)CLIB_HEAP_CAPACITY;
    clib_heap_block *block;

    if (chunk_size < (size_t)CLIB_HEAP_CHUNK_MIN) {
        chunk_size = (size_t)CLIB_HEAP_CHUNK_MIN;
    }
    if (chunk_size > (size_t)CLIB_HEAP_CHUNK_MAX) {
        chunk_size = (size_t)CLIB_HEAP_CHUNK_MAX;
    }
    if (chunk_size < need + header_size) {
        chunk_size = need + header_size;
    }

    chunk_size = clib_align_up(chunk_size, CLIB_HEAP_ALIGN);
    block = (clib_heap_block *)cleonos_sys_user_heap_alloc((u64)chunk_size);
    if (block == (clib_heap_block *)0) {
        return (clib_heap_block *)0;
    }

    block->size = chunk_size - header_size;
    block->used = 0U;
    block->magic = CLIB_HEAP_MAGIC;
    block->next = (clib_heap_block *)0;
    block->prev = clib_heap_tail;

    if (clib_heap_tail != (clib_heap_block *)0) {
        clib_heap_tail->next = block;
    } else {
        clib_heap_head = block;
    }

    clib_heap_tail = block;
    return block;
}

__attribute__((weak)) void *malloc(size_t size) {
    clib_heap_block *current;
    size_t need;
    size_t header_size = clib_heap_header_size();

    if (size == 0U) {
        return (void *)0;
    }

    need = clib_align_up(size, CLIB_HEAP_ALIGN);
    current = clib_heap_head;
    while (current != (clib_heap_block *)0) {
        if (current->magic == CLIB_HEAP_MAGIC && current->used == 0U && current->size >= need) {
            clib_heap_split(current, need);
            current->used = 1U;
            return (void *)(((unsigned char *)(void *)current) + header_size);
        }
        current = current->next;
    }

    current = clib_heap_request_chunk(need);
    if (current == (clib_heap_block *)0) {
        return (void *)0;
    }

    clib_heap_split(current, need);
    current->used = 1U;
    return (void *)(((unsigned char *)(void *)current) + header_size);
}

__attribute__((weak)) void free(void *ptr) {
    clib_heap_block *block = clib_heap_block_from_ptr(ptr);

    if (block == (clib_heap_block *)0) {
        return;
    }

    block->used = 0U;
    clib_heap_merge_next(block);
    if (block->prev != (clib_heap_block *)0) {
        clib_heap_merge_next(block->prev);
    }
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
    clib_heap_block *block;
    void *out;
    size_t old_size;

    if (ptr == (void *)0) {
        return malloc(size);
    }

    if (size == 0U) {
        free(ptr);
        return (void *)0;
    }

    block = clib_heap_block_from_ptr(ptr);
    if (block == (clib_heap_block *)0) {
        return (void *)0;
    }

    old_size = block->size;
    size = clib_align_up(size, CLIB_HEAP_ALIGN);
    if (old_size >= size) {
        clib_heap_split(block, size);
        return ptr;
    }

    if (block->next != (clib_heap_block *)0 && block->next->used == 0U) {
        clib_heap_merge_next(block);
        if (block->size >= size) {
            clib_heap_split(block, size);
            return ptr;
        }
    }

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
