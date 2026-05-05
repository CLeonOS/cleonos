#include <cleonos_syscall.h>

#include <stddef.h>
#include <string.h>

#ifndef CLEONOS_APP_HEAP_CAPACITY
#define CLEONOS_APP_HEAP_CAPACITY (2U * 1024U * 1024U)
#endif

#define CLEONOS_APP_HEAP_ALIGN 16U
#define CLEONOS_APP_HEAP_MAGIC 0xC10A5A7EU
#define CLEONOS_APP_HEAP_CHUNK_MIN (64U * 1024U)
#define CLEONOS_APP_HEAP_CHUNK_MAX (4U * 1024U * 1024U)

typedef struct cleonos_app_heap_block {
    size_t size;
    unsigned int used;
    unsigned int magic;
    struct cleonos_app_heap_block *next;
    struct cleonos_app_heap_block *prev;
} cleonos_app_heap_block;

static cleonos_app_heap_block *cleonos_app_heap_head = (cleonos_app_heap_block *)0;
static cleonos_app_heap_block *cleonos_app_heap_tail = (cleonos_app_heap_block *)0;

static size_t cleonos_app_heap_align_up(size_t value, size_t align) {
    size_t mask;

    if (align == 0U) {
        return value;
    }

    mask = align - 1U;
    return (value + mask) & ~mask;
}

static size_t cleonos_app_heap_header_size(void) {
    return cleonos_app_heap_align_up(sizeof(cleonos_app_heap_block), CLEONOS_APP_HEAP_ALIGN);
}

static int cleonos_app_heap_blocks_are_adjacent(const cleonos_app_heap_block *block,
                                                const cleonos_app_heap_block *next) {
    size_t header_size = cleonos_app_heap_header_size();
    const unsigned char *expected;

    if (block == (const cleonos_app_heap_block *)0 || next == (const cleonos_app_heap_block *)0) {
        return 0;
    }

    expected = ((const unsigned char *)(const void *)block) + header_size + block->size;
    return (expected == (const unsigned char *)(const void *)next) ? 1 : 0;
}

static void cleonos_app_heap_split(cleonos_app_heap_block *block, size_t need) {
    size_t header_size = cleonos_app_heap_header_size();
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
    } else {
        cleonos_app_heap_tail = next;
    }

    block->next = next;
    block->size = need;
}

static void cleonos_app_heap_merge_next(cleonos_app_heap_block *block) {
    size_t header_size = cleonos_app_heap_header_size();
    cleonos_app_heap_block *next;

    if (block == (cleonos_app_heap_block *)0 || block->used != 0U) {
        return;
    }

    next = block->next;
    if (next == (cleonos_app_heap_block *)0 || next->magic != CLEONOS_APP_HEAP_MAGIC || next->used != 0U ||
        cleonos_app_heap_blocks_are_adjacent(block, next) == 0) {
        return;
    }

    block->size += header_size + next->size;
    block->next = next->next;
    if (block->next != (cleonos_app_heap_block *)0) {
        block->next->prev = block;
    } else {
        cleonos_app_heap_tail = block;
    }
}

static cleonos_app_heap_block *cleonos_app_heap_block_from_ptr(void *ptr) {
    size_t header_size = cleonos_app_heap_header_size();
    cleonos_app_heap_block *current;

    if (ptr == (void *)0) {
        return (cleonos_app_heap_block *)0;
    }

    current = cleonos_app_heap_head;
    while (current != (cleonos_app_heap_block *)0) {
        void *payload = (void *)(((unsigned char *)(void *)current) + header_size);

        if (current->magic != CLEONOS_APP_HEAP_MAGIC) {
            return (cleonos_app_heap_block *)0;
        }

        if (payload == ptr && current->used != 0U) {
            return current;
        }

        current = current->next;
    }

    return (cleonos_app_heap_block *)0;
}

static cleonos_app_heap_block *cleonos_app_heap_request_chunk(size_t need) {
    size_t header_size = cleonos_app_heap_header_size();
    size_t chunk_size = (size_t)CLEONOS_APP_HEAP_CAPACITY;
    cleonos_app_heap_block *block;

    if (chunk_size < (size_t)CLEONOS_APP_HEAP_CHUNK_MIN) {
        chunk_size = (size_t)CLEONOS_APP_HEAP_CHUNK_MIN;
    }
    if (chunk_size > (size_t)CLEONOS_APP_HEAP_CHUNK_MAX) {
        chunk_size = (size_t)CLEONOS_APP_HEAP_CHUNK_MAX;
    }
    if (chunk_size < need + header_size) {
        chunk_size = need + header_size;
    }

    chunk_size = cleonos_app_heap_align_up(chunk_size, CLEONOS_APP_HEAP_ALIGN);
    block = (cleonos_app_heap_block *)cleonos_sys_vm_alloc((u64)chunk_size, CLEONOS_VM_FLAG_READ | CLEONOS_VM_FLAG_WRITE);
    if (block == (cleonos_app_heap_block *)0) {
        block = (cleonos_app_heap_block *)cleonos_sys_user_heap_alloc((u64)chunk_size);
    }
    if (block == (cleonos_app_heap_block *)0) {
        return (cleonos_app_heap_block *)0;
    }

    block->size = chunk_size - header_size;
    block->used = 0U;
    block->magic = CLEONOS_APP_HEAP_MAGIC;
    block->next = (cleonos_app_heap_block *)0;
    block->prev = cleonos_app_heap_tail;

    if (cleonos_app_heap_tail != (cleonos_app_heap_block *)0) {
        cleonos_app_heap_tail->next = block;
    } else {
        cleonos_app_heap_head = block;
    }

    cleonos_app_heap_tail = block;
    return block;
}

void *malloc(size_t size) {
    cleonos_app_heap_block *current;
    size_t need;
    size_t header_size = cleonos_app_heap_header_size();

    if (size == 0U) {
        return (void *)0;
    }

    need = cleonos_app_heap_align_up(size, CLEONOS_APP_HEAP_ALIGN);
    current = cleonos_app_heap_head;
    while (current != (cleonos_app_heap_block *)0) {
        if (current->magic == CLEONOS_APP_HEAP_MAGIC && current->used == 0U && current->size >= need) {
            cleonos_app_heap_split(current, need);
            current->used = 1U;
            return (void *)(((unsigned char *)(void *)current) + header_size);
        }
        current = current->next;
    }

    current = cleonos_app_heap_request_chunk(need);
    if (current == (cleonos_app_heap_block *)0) {
        return (void *)0;
    }

    cleonos_app_heap_split(current, need);
    current->used = 1U;
    return (void *)(((unsigned char *)(void *)current) + header_size);
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

    (void)memcpy(out, ptr, (old_size < size) ? old_size : size);
    free(ptr);
    return out;
}
