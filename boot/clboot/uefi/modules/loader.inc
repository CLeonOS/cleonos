static uint64_t clboot_align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1ULL);
}

static uint64_t clboot_align_up(uint64_t value, uint64_t align) {
    return (value + align - 1ULL) & ~(align - 1ULL);
}

static int clboot_load_elf64(const struct clboot_loaded_file *file, struct clboot_kernel_image *out) {
    const struct elf64_ehdr *eh;
    uint64_t min_vaddr = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t max_vaddr = 0ULL;
    uint64_t map_start;
    uint64_t map_end;
    uint64_t image_size;
    uint64_t raw_phys;
    uint64_t phys_base;
    uint16_t i;

    if (file->size < sizeof(*eh)) {
        return 0;
    }
    eh = (const struct elf64_ehdr *)file->data;
    if (eh->ident[0] != 0x7FU || eh->ident[1] != 'E' || eh->ident[2] != 'L' || eh->ident[3] != 'F' ||
        eh->ident[4] != 2U || eh->machine != 0x3EU || eh->phentsize != sizeof(struct elf64_phdr)) {
        return 0;
    }

    for (i = 0U; i < eh->phnum; i++) {
        const struct elf64_phdr *ph = (const struct elf64_phdr *)((const UINT8 *)file->data + eh->phoff +
                                                                 ((uint64_t)i * eh->phentsize));
        if (ph->type != 1U || ph->memsz == 0ULL) {
            continue;
        }
        if (ph->vaddr < min_vaddr) {
            min_vaddr = ph->vaddr;
        }
        if (ph->vaddr + ph->memsz > max_vaddr) {
            max_vaddr = ph->vaddr + ph->memsz;
        }
    }

    if (min_vaddr == 0xFFFFFFFFFFFFFFFFULL || max_vaddr <= min_vaddr) {
        return 0;
    }

    map_start = clboot_align_down(min_vaddr, 0x1000ULL);
    map_end = clboot_align_up(max_vaddr, 0x1000ULL);
    image_size = map_end - map_start;
    if (clboot_alloc_pages(EFI_SIZE_TO_PAGES(image_size + 0x200000ULL), &raw_phys) == 0) {
        return 0;
    }
    phys_base = clboot_align_up(raw_phys, 0x200000ULL);
    clboot_memset((void *)(UINTN)phys_base, 0, (UINTN)image_size);

    for (i = 0U; i < eh->phnum; i++) {
        const struct elf64_phdr *ph = (const struct elf64_phdr *)((const UINT8 *)file->data + eh->phoff +
                                                                 ((uint64_t)i * eh->phentsize));
        UINT8 *dst;
        if (ph->type != 1U || ph->memsz == 0ULL) {
            continue;
        }
        if (ph->offset + ph->filesz > file->size) {
            return 0;
        }
        dst = (UINT8 *)(UINTN)(phys_base + (ph->vaddr - map_start));
        clboot_memset(dst, 0, (UINTN)ph->memsz);
        clboot_memcpy(dst, (const UINT8 *)file->data + ph->offset, (UINTN)ph->filesz);
    }

    out->entry = eh->entry;
    out->phys_base = phys_base;
    out->virt_base = map_start;
    out->map_start = map_start;
    out->map_end = map_end;
    return 1;
}

static void clboot_map_2m(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t size) {
    uint64_t end = virt + size;

    while (virt < end) {
        uint64_t pml4_i = (virt >> 39U) & 0x1FFULL;
        uint64_t pdpt_i = (virt >> 30U) & 0x1FFULL;
        uint64_t pd_i = (virt >> 21U) & 0x1FFULL;
        uint64_t pdpt_phys;
        uint64_t pd_phys;
        uint64_t *pdpt;
        uint64_t *pd;

        if ((pml4[pml4_i] & CLBOOT_PT_PRESENT) == 0ULL) {
            if (clboot_alloc_pages(1U, &pdpt_phys) == 0) {
                clboot_halt(L"CLBoot: page table allocation failed");
            }
            pml4[pml4_i] = pdpt_phys | CLBOOT_PT_PRESENT | CLBOOT_PT_WRITE;
        }
        pdpt = (uint64_t *)(UINTN)(pml4[pml4_i] & 0x000FFFFFFFFFF000ULL);
        if ((pdpt[pdpt_i] & CLBOOT_PT_PRESENT) == 0ULL) {
            if (clboot_alloc_pages(1U, &pd_phys) == 0) {
                clboot_halt(L"CLBoot: page table allocation failed");
            }
            pdpt[pdpt_i] = pd_phys | CLBOOT_PT_PRESENT | CLBOOT_PT_WRITE;
        }
        pd = (uint64_t *)(UINTN)(pdpt[pdpt_i] & 0x000FFFFFFFFFF000ULL);
        pd[pd_i] = (phys & 0x000FFFFFFFFFF000ULL) | CLBOOT_PT_PRESENT | CLBOOT_PT_WRITE | CLBOOT_PT_PS;

        virt += 0x200000ULL;
        phys += 0x200000ULL;
    }
}

static uint64_t clboot_build_page_tables(const struct clboot_kernel_image *kernel, uint64_t fb_base, uint64_t fb_size) {
    uint64_t pml4_phys;
    uint64_t *pml4;
    uint64_t kernel_phys = clboot_align_down(kernel->phys_base, 0x200000ULL);
    uint64_t kernel_virt = clboot_align_down(kernel->map_start, 0x200000ULL);
    uint64_t kernel_size = clboot_align_up((kernel->phys_base - kernel_phys) + (kernel->map_end - kernel->map_start),
                                           0x200000ULL);

    if (clboot_alloc_pages(1U, &pml4_phys) == 0) {
        clboot_halt(L"CLBoot: PML4 allocation failed");
    }
    pml4 = (uint64_t *)(UINTN)pml4_phys;

    clboot_map_2m(pml4, 0ULL, 0ULL, CLBOOT_BOOT_PHYS_MAP_SIZE);
    clboot_map_2m(pml4, CLBOOT_HHDM_OFFSET, 0ULL, CLBOOT_BOOT_PHYS_MAP_SIZE);
    clboot_map_2m(pml4, kernel_virt, kernel_phys, kernel_size);
    if (fb_base != 0ULL && fb_size != 0ULL) {
        uint64_t fb_phys = clboot_align_down(fb_base, 0x200000ULL);
        uint64_t fb_map_size = clboot_align_up((fb_base - fb_phys) + fb_size, 0x200000ULL);
        clboot_map_2m(pml4, CLBOOT_HHDM_OFFSET + fb_phys, fb_phys, fb_map_size);
        clboot_map_2m(pml4, fb_phys, fb_phys, fb_map_size);
    }

    return pml4_phys;
}
