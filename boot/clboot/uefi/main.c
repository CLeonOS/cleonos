#include "clboot_uefi.h"
#include "../include/clboot_protocol.h"

#define CLBOOT_CONFIG_PATH L"\\EFI\\CLEONOS\\clboot.conf"
#define CLBOOT_DEFAULT_CMDLINE "clks.boot=iso clks.bootloader=clboot"
#define CLBOOT_KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL
#define CLBOOT_MEMMAP_MAX_ENTRIES 512U
#define CLBOOT_BOOT_PHYS_MAP_SIZE 0x100000000ULL
#define CLBOOT_KERNEL_STACK_PAGES 16U
#define CLBOOT_LOG_BYTES 8192U
#define CLBOOT_MENU_TIMEOUT_SECONDS 5U
#define CLBOOT_MENU_MAX_ENTRIES 16U
#define CLBOOT_MENU_TITLE_MAX 64U
#define CLBOOT_MENU_HINT_MAX 96U
#define CLBOOT_MENU_CMDLINE_MAX 512U
#define CLBOOT_MENU_PATH_MAX 128U
#define CLBOOT_TEXT_ATTR_NORMAL 0x07U
#define CLBOOT_TEXT_ATTR_TITLE 0x0FU
#define CLBOOT_TEXT_ATTR_ACCENT 0x0BU
#define CLBOOT_TEXT_ATTR_SELECTED 0x70U
#define CLBOOT_TEXT_ATTR_WARN 0x0EU
#define CLBOOT_TEXT_ATTR_ERROR 0x0CU
#define CLBOOT_TEXT_ATTR_DIM 0x07U
#define CLBOOT_TEXT_ATTR_OK 0x0AU
#define CLBOOT_TEXT_ATTR_PANEL 0x0FU

#define CLBOOT_PT_PRESENT (1ULL << 0U)
#define CLBOOT_PT_WRITE (1ULL << 1U)
#define CLBOOT_PT_PS (1ULL << 7U)

#define CLBOOT_GDT_CODE_SELECTOR 0x08U
#define CLBOOT_GDT_DATA_SELECTOR 0x10U

EFI_GUID gEfiLoadedImageProtocolGuid = {0x5B1B31A1U, 0x9562U, 0x11D2U, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
EFI_GUID gEfiSimpleFileSystemProtocolGuid = {0x0964E5B22U, 0x6459U, 0x11D2U, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
EFI_GUID gEfiGraphicsOutputProtocolGuid = {0x9042A9DEU, 0x23DCU, 0x4A38U, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A}};

typedef void (*clboot_kernel_entry_t)(uint64_t magic, struct clboot_info *info);

struct clboot_menu_entry {
    CHAR16 title[CLBOOT_MENU_TITLE_MAX];
    CHAR16 hint[CLBOOT_MENU_HINT_MAX];
    CHAR16 kernel_path[CLBOOT_MENU_PATH_MAX];
    CHAR16 ramdisk_path[CLBOOT_MENU_PATH_MAX];
    CHAR16 source[CLBOOT_MENU_HINT_MAX];
    CHAR16 validation[CLBOOT_MENU_HINT_MAX];
    int kernel_ok;
    int ramdisk_ok;
    int valid;
    char cmdline[CLBOOT_MENU_CMDLINE_MAX];
};

struct clboot_loaded_file {
    void *data;
    uint64_t size;
};

struct elf64_ehdr {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

struct clboot_kernel_image {
    uint64_t entry;
    uint64_t phys_base;
    uint64_t virt_base;
    uint64_t map_start;
    uint64_t map_end;
};

struct clboot_menu_config {
    struct clboot_menu_entry entries[CLBOOT_MENU_MAX_ENTRIES];
    UINTN count;
    UINTN default_index;
    UINTN timeout_seconds;
    char global_cmdline[CLBOOT_MENU_CMDLINE_MAX];
};

struct clboot_gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static EFI_SYSTEM_TABLE *clboot_st;
static EFI_BOOT_SERVICES *clboot_bs;
static EFI_HANDLE clboot_image;
static char clboot_bootlog[CLBOOT_LOG_BYTES];
static UINTN clboot_bootlog_used = 0U;
static UINTN clboot_bootlog_entries = 0U;
static struct clboot_menu_config clboot_menu_config_storage;
static uint64_t clboot_gdt[3] = {
    0x0000000000000000ULL,
    0x00AF9A000000FFFFULL,
    0x00CF92000000FFFFULL,
};
static struct clboot_gdt_ptr clboot_gdt_ptr = {
    (uint16_t)(sizeof(clboot_gdt) - 1U),
    (uint64_t)(UINTN)&clboot_gdt[0],
};

static void clboot_print(const CHAR16 *text);
static int clboot_gfx_available(void);
static void clboot_gfx_draw_progress(UINTN percent, CHAR16 *label);
static void clboot_gfx_draw_halt(CHAR16 *message);
static int clboot_gfx_console_active(void);
static void clboot_gfx_console_begin(CHAR16 *subtitle);
static void clboot_gfx_console_clear(void);
static void clboot_gfx_console_enable_cursor(int visible);
static void clboot_gfx_console_print(const CHAR16 *text);
static void clboot_gfx_console_set_attr(UINTN attr);

#include "modules/base.inc"
#include "modules/ui.inc"
#include "modules/fs.inc"
#include "modules/loader.inc"
#include "modules/gfx.inc"
#include "modules/shell.inc"
#include "modules/menu.inc"
#include "modules/boot.inc"
