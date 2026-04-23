#ifndef CLKS_PMM_H
#define CLKS_PMM_H

#include <clks/limine.h>
#include <clks/types.h>

#define CLKS_PAGE_SIZE 4096ULL

struct clks_pmm_stats {
    u64 managed_pages;
    u64 free_pages;
    u64 used_pages;
    u64 dropped_pages;
};

void clks_pmm_init(const struct limine_memmap_response *memmap);
u64 clks_pmm_alloc_page(void);
void clks_pmm_free_page(u64 phys_addr);
struct clks_pmm_stats clks_pmm_get_stats(void);

#endif