static uint64_t clboot_memmap_type(UINT32 efi_type) {
    switch (efi_type) {
    case EFI_CONVENTIONAL_MEMORY:
        return CLBOOT_MEMMAP_USABLE;
    case EFI_ACPI_RECLAIM_MEMORY:
        return CLBOOT_MEMMAP_ACPI_RECLAIMABLE;
    case EFI_ACPI_MEMORY_NVS:
        return CLBOOT_MEMMAP_ACPI_NVS;
    case EFI_UNUSABLE_MEMORY:
        return CLBOOT_MEMMAP_BAD_MEMORY;
    case EFI_LOADER_CODE:
    case EFI_LOADER_DATA:
    case EFI_BOOT_SERVICES_CODE:
    case EFI_BOOT_SERVICES_DATA:
        return CLBOOT_MEMMAP_BOOTLOADER_RECLAIMABLE;
    default:
        return CLBOOT_MEMMAP_RESERVED;
    }
}

static void __attribute__((noreturn)) clboot_enter_kernel(uint64_t entry, uint64_t cr3, struct clboot_info *info,
                                                          uint64_t stack_top) {
    __asm__ volatile("lgdt (%0)\n"
                     "mov %1, %%cr3\n"
                     "mov $0x10, %%ax\n"
                     "mov %%ax, %%ds\n"
                     "mov %%ax, %%es\n"
                     "mov %%ax, %%ss\n"
                     "mov %2, %%rsp\n"
                     "and $-16, %%rsp\n"
                     "sub $8, %%rsp\n"
                     "xor %%rbp, %%rbp\n"
                     "mov %3, %%rdi\n"
                     "mov %4, %%rsi\n"
                     "pushq $0x08\n"
                     "pushq %5\n"
                     "lretq\n"
                     :
                     : "r"(&clboot_gdt_ptr), "r"(cr3), "r"(stack_top), "r"(CLBOOT_MAGIC), "r"(info), "r"(entry)
                     : "memory", "rax", "rdi", "rsi");
    __builtin_unreachable();
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    struct clboot_loaded_file kernel_file;
    struct clboot_loaded_file ramdisk_file;
    struct clboot_kernel_image kernel;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0;
    struct clboot_info *info;
    struct clboot_framebuffer *fb;
    struct clboot_module *module;
    char *cmdline;
    UINTN mmap_size = 0U;
    UINTN mmap_key = 0U;
    UINTN desc_size = 0U;
    UINT32 desc_version = 0U;
    EFI_MEMORY_DESCRIPTOR *mmap;
    struct clboot_memmap_entry *cl_mmap;
    UINTN mmap_count;
    UINTN cl_mmap_capacity;
    UINTN i;
    UINTN menu_index;
    struct clboot_menu_config *menu_config;
    struct clboot_menu_entry *boot_entry;
    uint64_t cr3;
    uint64_t stack_phys;
    uint64_t stack_top;

    clboot_st = SystemTable;
    clboot_bs = SystemTable->BootServices;
    clboot_image = ImageHandle;

    clboot_log_append_ascii("CLBoot UEFI starting\n");
    (void)clboot_gfx_init();
    if (clboot_gfx_available() != 0) {
        clboot_enable_cursor(0);
    } else {
        clboot_select_text_mode();
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        clboot_clear_screen();
    }

    menu_config = &clboot_menu_config_storage;
    clboot_menu_config_init_defaults(menu_config);
    clboot_menu_parse_config(menu_config);
    clboot_menu_scan_kernel_dir(menu_config);
    clboot_menu_validate(menu_config);
    menu_index = clboot_boot_menu_select(menu_config);
    boot_entry = &menu_config->entries[menu_index];
    if (clboot_gfx_available() == 0) {
        clboot_draw_compact_header(L"Boot progress");
        clboot_print(L" Preparing kernel handoff.\r\n");
    }
    clboot_status(L"UEFI boot services online");
    clboot_status(boot_entry->title);
    clboot_draw_progress(5U, L"connecting devices");
    clboot_connect_all_controllers();

    clboot_draw_progress(15U, L"reading kernel");
    if (clboot_read_file(boot_entry->kernel_path, &kernel_file) == 0) {
        clboot_halt(L"CLBoot: cannot read kernel ELF");
    }
    clboot_status(L"kernel ELF loaded");

    clboot_draw_progress(25U, L"loading kernel ELF");
    if (clboot_load_elf64(&kernel_file, &kernel) == 0) {
        clboot_halt(L"CLBoot: invalid kernel ELF");
    }
    clboot_status(L"kernel ELF mapped");

    clboot_draw_progress(35U, L"reading ramdisk");
    if (clboot_path_is_empty_or_none(boot_entry->ramdisk_path) != 0) {
        ramdisk_file.data = (void *)0;
        ramdisk_file.size = 0ULL;
        clboot_status(L"ramdisk skipped");
    } else if (clboot_read_file(boot_entry->ramdisk_path, &ramdisk_file) == 0) {
        clboot_halt(L"CLBoot: cannot read ramdisk");
    } else {
        clboot_status(L"ramdisk loaded");
    }

    clboot_draw_progress(45U, L"loading command line");
    cmdline = clboot_cmdline_build(menu_config->global_cmdline, boot_entry->cmdline);
    if (cmdline == (char *)0) {
        clboot_halt(L"CLBoot: cmdline allocation failed");
    }
    clboot_status(L"command line ready");

    clboot_draw_progress(52U, L"probing framebuffer");
    (void)clboot_bs->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, (void *)0, (void **)&gop);

    clboot_draw_progress(58U, L"allocating boot info");
    if (clboot_alloc_pool(sizeof(*info), (void **)&info) == 0 ||
        clboot_alloc_pool(sizeof(*fb), (void **)&fb) == 0 ||
        (ramdisk_file.size != 0ULL && clboot_alloc_pool(sizeof(*module), (void **)&module) == 0)) {
        clboot_halt(L"CLBoot: boot info allocation failed");
    }

    if (gop != (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0 && gop->Mode != (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *)0 &&
        gop->Mode->Info != (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *)0) {
        fb->address = gop->Mode->FrameBufferBase;
        fb->width = gop->Mode->Info->HorizontalResolution;
        fb->height = gop->Mode->Info->VerticalResolution;
        fb->pitch = gop->Mode->Info->PixelsPerScanLine * 4ULL;
        fb->bpp = 32U;
        fb->memory_model = 1U;
        fb->red_mask_size = 8U;
        fb->red_mask_shift = 16U;
        fb->green_mask_size = 8U;
        fb->green_mask_shift = 8U;
        fb->blue_mask_size = 8U;
        fb->blue_mask_shift = 0U;
    }

    if (ramdisk_file.size != 0ULL) {
        module->address = (uint64_t)(UINTN)ramdisk_file.data;
        module->size = ramdisk_file.size;
        module->path = (uint64_t)(UINTN)"ramdisk";
        module->cmdline = (uint64_t)(UINTN)"ramdisk";
    }

    clboot_draw_progress(64U, L"allocating memory map");
    cl_mmap_capacity = CLBOOT_MEMMAP_MAX_ENTRIES;
    if (clboot_alloc_pool(sizeof(*cl_mmap) * cl_mmap_capacity, (void **)&cl_mmap) == 0) {
        clboot_halt(L"CLBoot: CL memmap allocation failed");
    }

    info->magic = CLBOOT_MAGIC;
    info->version = CLBOOT_VERSION;
    info->hhdm_offset = CLBOOT_HHDM_OFFSET;
    info->kernel_entry = kernel.entry;
    info->kernel_virtual_base = kernel.virt_base;
    info->kernel_physical_base = kernel.phys_base;
    info->cmdline = (uint64_t)(UINTN)cmdline;
    info->framebuffer = (uint64_t)(UINTN)fb;
    info->memmap_entries = (uint64_t)(UINTN)cl_mmap;
    info->memmap_count = 0ULL;
    info->modules = (ramdisk_file.size != 0ULL) ? (uint64_t)(UINTN)module : 0ULL;
    info->module_count = (ramdisk_file.size != 0ULL) ? 1ULL : 0ULL;
    info->rsdp = 0ULL;
    info->bootlog = (uint64_t)(UINTN)clboot_bootlog;
    info->bootlog_size = (uint64_t)clboot_bootlog_used;
    info->bootlog_entry_count = (uint64_t)clboot_bootlog_entries;

    clboot_draw_progress(70U, L"allocating kernel stack");
    if (clboot_alloc_pages_below(CLBOOT_KERNEL_STACK_PAGES, CLBOOT_BOOT_PHYS_MAP_SIZE - 1ULL, &stack_phys) == 0) {
        clboot_halt(L"CLBoot: kernel stack allocation failed");
    }
    stack_top = stack_phys + ((uint64_t)CLBOOT_KERNEL_STACK_PAGES << 12U);

    clboot_draw_progress(78U, L"building page tables");
    cr3 = clboot_build_page_tables(&kernel, fb->address, gop != (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0 ? gop->Mode->FrameBufferSize : 0ULL);

    clboot_draw_progress(86U, L"collecting UEFI memory map");
    mmap_size = 128U * 1024U;
    if (clboot_alloc_pool(mmap_size, (void **)&mmap) == 0) {
        clboot_halt(L"CLBoot: memory map allocation failed");
    }
    if (clboot_bs->GetMemoryMap(&mmap_size, mmap, &mmap_key, &desc_size, &desc_version) != EFI_SUCCESS) {
        clboot_halt(L"CLBoot: GetMemoryMap failed");
    }
    mmap_count = mmap_size / desc_size;
    if (mmap_count > cl_mmap_capacity) {
        clboot_halt(L"CLBoot: memory map too large");
    }
    for (i = 0U; i < mmap_count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mmap + (i * desc_size));
        cl_mmap[i].base = desc->PhysicalStart;
        cl_mmap[i].length = desc->NumberOfPages << 12U;
        cl_mmap[i].type = clboot_memmap_type(desc->Type);
    }
    info->memmap_count = mmap_count;
    info->bootlog_size = (uint64_t)clboot_bootlog_used;
    info->bootlog_entry_count = (uint64_t)clboot_bootlog_entries;

    clboot_draw_progress(94U, L"exiting boot services");
    if (clboot_bs->ExitBootServices(ImageHandle, mmap_key) != EFI_SUCCESS) {
        mmap_size = 128U * 1024U;
        if (clboot_bs->GetMemoryMap(&mmap_size, mmap, &mmap_key, &desc_size, &desc_version) != EFI_SUCCESS) {
            clboot_halt(L"CLBoot: GetMemoryMap retry failed");
        }
        mmap_count = mmap_size / desc_size;
        if (mmap_count > cl_mmap_capacity) {
            clboot_halt(L"CLBoot: memory map retry too large");
        }
        for (i = 0U; i < mmap_count; i++) {
            EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mmap + (i * desc_size));
            cl_mmap[i].base = desc->PhysicalStart;
            cl_mmap[i].length = desc->NumberOfPages << 12U;
            cl_mmap[i].type = clboot_memmap_type(desc->Type);
        }
        info->memmap_count = mmap_count;
        info->bootlog_size = (uint64_t)clboot_bootlog_used;
        info->bootlog_entry_count = (uint64_t)clboot_bootlog_entries;
        if (clboot_bs->ExitBootServices(ImageHandle, mmap_key) != EFI_SUCCESS) {
            clboot_halt(L"CLBoot: ExitBootServices failed");
        }
    }

    clboot_log_append_ascii("Entering kernel\n");
    clboot_draw_progress(100U, L"entering kernel");
    clboot_enter_kernel(kernel.entry, cr3, info, stack_top);
    return EFI_SUCCESS;
}
