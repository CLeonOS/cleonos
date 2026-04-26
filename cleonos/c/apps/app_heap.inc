#include <stddef.h>
#include <string.h>

#ifndef CLEONOS_APP_HEAP_CAPACITY
#define CLEONOS_APP_HEAP_CAPACITY (2U * 1024U * 1024U)
#endif

#define CLEONOS_APP_HEAP_ALIGN 8U
#define CLEONOS_APP_HEAP_MAGIC 0xC10A5A7EU

typedef union cleonos_app_heap_storage {
    unsigned long long align;
    unsigned char bytes[CLEONOS_APP_HEAP_CAPACITY];
} cleonos_app_heap_storage;

typedef struct cleonos_app_heap_block {
    size_t size;
    unsigned int used;
    unsigned int magic;
    struct cleonos_app_heap_block *next;
    struct cleonos_app_heap_block *prev;
} cleonos_app_heap_block;

static cleonos_app_heap_storage cleonos_app_heap;
static cleonos_app_heap_block *cleonos_app_heap_head = (cleonos_app_heap_block *)0;
static int cleonos_app_heap_ready = 0;

static size_t cleonos_app_heap_align_up(size_t value, size_t align) {
    size_t mask;

    if (align == 0U) {
        return value;
    }

    mask = align - 1U;
    return (value + mask) & ~mask;
}

static void cleonos_app_heap_init(void) {
    size_t header_size = cleonos_app_heap_align_up(sizeof(cleonos_app_heap_block), CLEONOS_APP_HEAP_ALIGN);

    if (cleonos_app_heap_ready != 0) {
        return;
    }

    if ((size_t)CLEONOS_APP_HEAP_CAPACITY <= header_size + CLEONOS_APP_HEAP_ALIGN) {
        return;
    }

    cleonos_app_heap_head = (cleonos_app_heap_block *)(void *)cleonos_app_heap.bytes;
    cleonos_app_heap_head->size = (size_t)CLEONOS_APP_HEAP_CAPACITY - header_size;
    cleonos_app_heap_head->used = 0U;
    cleonos_app_heap_head->magic = CLEONOS_APP_HEAP_MAGIC;
    cleonos_app_heap_head->next = (cleonos_app_heap_block *)0;
    cleonos_app_heap_head->prev = (cleonos_app_heap_block *)0;
    cleonos_app_heap_ready = 1;
}

static void cleonos_app_heap_split(cleonos_app_heap_block *block, size_t need) {
    size_t header_size = cleonos_app_heap_align_up(sizeof(cleonos_app_heap_block), CLEONOS_APP_HEAP_ALIGN);
    unsigned char *next_addr;
    cleonos_app_heap_block *next;

    if (block == (cleonos_app_heap_block *)0 || block->size <= need + header_size + CLEONOS_APP_HEAP_ALIGN) {
        return;
    }

    next_addr = ((unsigned char *)(void *)block) + header_size + need;
    next = (cleonos_app_heap_block *)(void *)next_addr;
    next->size = block->size - need - header_size;
    next->used = 0U;
    next->magic = CLEONOS_APP_HEAP_MAGIC;
    next->next = block->next;
    next->prev = block;
    if (next->next != (cleonos_app_heap_block *)0) {
        next->next->prev = next;
    }

    block->next = next;
    block->size = need;
}

static void cleonos_app_heap_merge_next(cleonos_app_heap_block *block) {
    size_t header_size = cleonos_app_heap_align_up(sizeof(cleonos_app_heap_block), CLEONOS_APP_HEAP_ALIGN);
    cleonos_app_heap_block *next;

    if (block == (cleonos_app_heap_block *)0 || block->used != 0U) {
        return;
    }

    next = block->next;
    if (next == (cleonos_app_heap_block *)0 || next->magic != CLEONOS_APP_HEAP_MAGIC || next->used != 0U) {
        return;
    }

    block->size += header_size + next->size;
    block->next = next->next;
    if (block->next != (cleonos_app_heap_block *)0) {
        block->next->prev = block;
    }
}

static cleonos_app_heap_block *cleonos_app_heap_block_from_ptr(void *ptr) {
    size_t header_size = cleonos_app_heap_align_up(sizeof(cleonos_app_heap_block), CLEONOS_APP_HEAP_ALIGN);
    unsigned char *raw = (unsigned char *)ptr;
    unsigned char *begin = cleonos_app_heap.bytes;
    unsigned char *end = cleonos_app_heap.bytes + (size_t)CLEONOS_APP_HEAP_CAPACITY;
    cleonos_app_heap_block *block;

    if (ptr == (void *)0 || raw < begin + header_size || raw >= end) {
        return (cleonos_app_heap_block *)0;
    }

    block = (cleonos_app_heap_block *)(void *)(raw - header_size);
    if (block->magic != CLEONOS_APP_HEAP_MAGIC || block->used == 0U) {
        return (cleonos_app_heap_block *)0;
    }

    return block;
}

void *malloc(size_t size) {
    cleonos_app_heap_block *current;
    size_t need = cleonos_app_heap_align_up(size, CLEONOS_APP_HEAP_ALIGN);
    size_t header_size = cleonos_app_heap_align_up(sizeof(cleonos_app_heap_block), CLEONOS_APP_HEAP_ALIGN);

    if (size == 0U) {
        return (void *)0;
    }

    cleonos_app_heap_init();
    if (cleonos_app_heap_ready == 0) {
        return (void *)0;
    }

    current = cleonos_app_heap_head;
    while (current != (cleonos_app_heap_block *)0) {
        if (current->magic == CLEONOS_APP_HEAP_MAGIC && current->used == 0U && current->size >= need) {
            cleonos_app_heap_split(current, need);
            current->used = 1U;
            return (void *)(((unsigned char *)(void *)current) + header_size);
        }
        current = current->next;
    }

    return (void *)0;
}

void free(void *ptr) {
    cleonos_app_heap_block *block = cleonos_app_heap_block_from_ptr(ptr);

    if (block == (cleonos_app_heap_block *)0) {
        return;
    }

    block->used = 0U;
    cleonos_app_heap_merge_next(block);
    if (block->prev != (cleonos_app_heap_block *)0) {
        cleonos_app_heap_merge_next(block->prev);
    }
}

void *calloc(size_t count, size_t size) {
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

void *realloc(void *ptr, size_t size) {
    cleonos_app_heap_block *block;
    void *out;
    size_t old_size;

    if (ptr == (void *)0) {
        return malloc(size);
    }

    if (size == 0U) {
        free(ptr);
        return (void *)0;
    }

    block = cleonos_app_heap_block_from_ptr(ptr);
    if (block == (cleonos_app_heap_block *)0) {
        return (void *)0;
    }

    old_size = block->size;
    size = cleonos_app_heap_align_up(size, CLEONOS_APP_HEAP_ALIGN);
    if (old_size >= size) {
        cleonos_app_heap_split(block, size);
        return ptr;
    }

    if (block->next != (cleonos_app_heap_block *)0 && block->next->used == 0U) {
        cleonos_app_heap_merge_next(block);
        if (block->size >= size) {
            cleonos_app_heap_split(block, size);
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
