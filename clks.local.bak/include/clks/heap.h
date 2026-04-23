#ifndef CLKS_HEAP_H
#define CLKS_HEAP_H

#include <clks/types.h>

struct clks_heap_stats {
    usize total_bytes;
    usize used_bytes;
    usize free_bytes;
    u64 alloc_count;
    u64 free_count;
};

void clks_heap_init(void);
void *clks_kmalloc(usize size);
void clks_kfree(void *ptr);
struct clks_heap_stats clks_heap_get_stats(void);

#endif