#ifndef CLBOOT_PROTOCOL_H
#define CLBOOT_PROTOCOL_H

#include <stdint.h>

#define CLBOOT_MAGIC 0x434C424F4F543031ULL
#define CLBOOT_VERSION 2U
#define CLBOOT_HHDM_OFFSET 0xFFFF800000000000ULL

#define CLBOOT_MEMMAP_USABLE 0ULL
#define CLBOOT_MEMMAP_RESERVED 1ULL
#define CLBOOT_MEMMAP_ACPI_RECLAIMABLE 2ULL
#define CLBOOT_MEMMAP_ACPI_NVS 3ULL
#define CLBOOT_MEMMAP_BAD_MEMORY 4ULL
#define CLBOOT_MEMMAP_BOOTLOADER_RECLAIMABLE 5ULL
#define CLBOOT_MEMMAP_KERNEL_AND_MODULES 6ULL
#define CLBOOT_MEMMAP_FRAMEBUFFER 7ULL

struct clboot_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct clboot_module {
    uint64_t address;
    uint64_t size;
    uint64_t path;
    uint64_t cmdline;
};

struct clboot_framebuffer {
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t reserved[7];
};

struct clboot_info {
    uint64_t magic;
    uint32_t version;
    uint32_t flags;
    uint64_t hhdm_offset;
    uint64_t kernel_entry;
    uint64_t kernel_virtual_base;
    uint64_t kernel_physical_base;
    uint64_t cmdline;
    uint64_t framebuffer;
    uint64_t memmap_entries;
    uint64_t memmap_count;
    uint64_t modules;
    uint64_t module_count;
    uint64_t rsdp;
    uint64_t bootlog;
    uint64_t bootlog_size;
    uint64_t bootlog_entry_count;
};

#endif
