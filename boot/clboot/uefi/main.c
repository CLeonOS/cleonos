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

static void *clboot_memset(void *dst, int value, UINTN size) {
    UINT8 *p = (UINT8 *)dst;
    while (size-- != 0U) {
        *p++ = (UINT8)value;
    }
    return dst;
}

static void *clboot_memcpy(void *dst, const void *src, UINTN size) {
    UINT8 *d = (UINT8 *)dst;
    const UINT8 *s = (const UINT8 *)src;
    while (size-- != 0U) {
        *d++ = *s++;
    }
    return dst;
}

static UINTN clboot_strlen(const char *text) {
    UINTN len = 0U;
    if (text == (const char *)0) {
        return 0U;
    }
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static int clboot_str_equal_n(const char *left, const char *right, UINTN len) {
    UINTN i;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    for (i = 0U; i < len; i++) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return right[len] == '\0' ? 1 : 0;
}

static void clboot_copy_ascii(char *dst, UINTN dst_size, const char *src) {
    UINTN i = 0U;

    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void clboot_copy_ascii_n(char *dst, UINTN dst_size, const char *src, UINTN src_len) {
    UINTN i = 0U;

    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && i < src_len && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static UINTN clboot_wstrlen(const CHAR16 *text) {
    UINTN len = 0U;
    if (text == (CHAR16 *)0) {
        return 0U;
    }
    while (text[len] != 0U) {
        len++;
    }
    return len;
}

static void clboot_copy_wide(CHAR16 *dst, UINTN dst_size, CHAR16 *src) {
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    if (src != (CHAR16 *)0) {
        while (i + 1U < dst_size && src[i] != 0U) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = 0U;
}

static void clboot_ascii_to_wide(CHAR16 *dst, UINTN dst_size, const char *src) {
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && src[i] != '\0') {
            dst[i] = (CHAR16)(unsigned char)src[i];
            i++;
        }
    }
    dst[i] = 0U;
}

static void clboot_ascii_n_to_wide(CHAR16 *dst, UINTN dst_size, const char *src, UINTN src_len) {
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && i < src_len && src[i] != '\0') {
            dst[i] = (CHAR16)(unsigned char)src[i];
            i++;
        }
    }
    dst[i] = 0U;
}

static void clboot_print_repeat(CHAR16 ch, UINTN count) {
    CHAR16 text[2];
    text[0] = ch;
    text[1] = 0U;

    while (count-- != 0U) {
        clboot_print(text);
    }
}

static void clboot_print_padded(const CHAR16 *text, UINTN width) {
    UINTN len = clboot_wstrlen(text);

    if (text != (CHAR16 *)0) {
        clboot_print(text);
    }
    if (len < width) {
        clboot_print_repeat(L' ', width - len);
    }
}

static void clboot_print_uint(UINTN value) {
    CHAR16 digits[24];
    CHAR16 out[24];
    UINTN count = 0U;
    UINTN i;

    if (value == 0U) {
        clboot_print(L"0");
        return;
    }

    while (value != 0U && count < (sizeof(digits) / sizeof(digits[0]))) {
        digits[count++] = (CHAR16)(L'0' + (value % 10U));
        value /= 10U;
    }

    for (i = 0U; i < count; i++) {
        out[i] = digits[count - i - 1U];
    }
    out[count] = 0U;
    clboot_print(out);
}

static void clboot_log_append_char(char ch) {
    if (clboot_bootlog_used + 1U >= CLBOOT_LOG_BYTES) {
        return;
    }
    clboot_bootlog[clboot_bootlog_used++] = ch;
    clboot_bootlog[clboot_bootlog_used] = '\0';
}

static void clboot_log_append_ascii(const char *text) {
    UINTN i = 0U;
    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        clboot_log_append_char(text[i++]);
    }
}

static void clboot_log_append_utf16_lossy(CHAR16 *text) {
    UINTN i = 0U;
    if (text == (CHAR16 *)0) {
        return;
    }
    while (text[i] != 0U) {
        CHAR16 ch = text[i++];
        if (ch == '\r') {
            continue;
        }
        clboot_log_append_char((ch >= 32U && ch < 127U) ? (char)ch : '?');
    }
}

static void clboot_print(const CHAR16 *text) {
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0) {
        (void)clboot_st->ConOut->OutputString(clboot_st->ConOut, (CHAR16 *)text);
    }
}

static void clboot_set_attr(UINTN attr) {
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->SetAttribute != (EFI_TEXT_SET_ATTRIBUTE)0) {
        (void)clboot_st->ConOut->SetAttribute(clboot_st->ConOut, attr);
    }
}

static void clboot_clear_screen(void) {
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->ClearScreen != (EFI_TEXT_CLEAR_SCREEN)0) {
        (void)clboot_st->ConOut->ClearScreen(clboot_st->ConOut);
    }
}

static void clboot_set_cursor(UINTN col, UINTN row) {
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->SetCursorPosition != (EFI_TEXT_SET_CURSOR_POSITION)0) {
        (void)clboot_st->ConOut->SetCursorPosition(clboot_st->ConOut, col, row);
    }
}

static void clboot_enable_cursor(int visible) {
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->EnableCursor != (EFI_TEXT_ENABLE_CURSOR)0) {
        (void)clboot_st->ConOut->EnableCursor(clboot_st->ConOut, visible ? 1U : 0U);
    }
}

static void clboot_select_text_mode(void) {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out;
    UINTN best_mode = 0U;
    UINTN best_cols = 0U;
    UINTN best_rows = 0U;
    INT32 mode;

    if (clboot_st == (EFI_SYSTEM_TABLE *)0 || clboot_st->ConOut == (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 ||
        clboot_st->ConOut->Mode == (SIMPLE_TEXT_OUTPUT_MODE *)0 ||
        clboot_st->ConOut->QueryMode == (EFI_TEXT_QUERY_MODE)0 ||
        clboot_st->ConOut->SetMode == (EFI_TEXT_SET_MODE)0) {
        return;
    }

    out = clboot_st->ConOut;
    for (mode = 0; mode < out->Mode->MaxMode; mode++) {
        UINTN cols = 0U;
        UINTN rows = 0U;
        if (out->QueryMode(out, (UINTN)mode, &cols, &rows) != EFI_SUCCESS) {
            continue;
        }
        if (cols < 80U || rows < 25U) {
            continue;
        }
        if ((cols * rows) > (best_cols * best_rows)) {
            best_mode = (UINTN)mode;
            best_cols = cols;
            best_rows = rows;
        }
    }

    if (best_cols >= 80U && best_rows >= 25U) {
        (void)out->SetMode(out, best_mode);
    }
}

static void clboot_log(CHAR16 *message) {
    clboot_log_append_utf16_lossy(message);
    clboot_log_append_char('\n');
    clboot_bootlog_entries++;
}

static void clboot_status(CHAR16 *message) {
    clboot_print(L"  ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
    clboot_print(L"* ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(message);
    clboot_print(L"\r\n");
    clboot_log(message);
}

static void clboot_draw_box_top(UINTN width) {
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  +");
    clboot_print_repeat(L'=', width);
    clboot_print(L"+\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_box_bottom(UINTN width) {
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  +");
    clboot_print_repeat(L'=', width);
    clboot_print(L"+\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_box_line(CHAR16 *left, CHAR16 *right, UINTN width) {
    UINTN payload_width = (width > 2U) ? (width - 2U) : width;
    UINTN left_width = payload_width;
    UINTN right_len = clboot_wstrlen(right);

    if (right_len + 2U < payload_width) {
        left_width = payload_width - right_len - 2U;
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  | ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print_padded(left, left_width);
    if (right != (CHAR16 *)0 && right_len > 0U && right_len + 2U < payload_width) {
        clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
        clboot_print(L"  ");
        clboot_print(right);
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    }
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" |\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_brand_header(CHAR16 *mode) {
    clboot_clear_screen();
    clboot_set_cursor(0U, 0U);
    clboot_print(L"\r\n");
    clboot_draw_box_top(76U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  | ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_TITLE);
    clboot_print_padded(L"CLeonOS CLBoot", 74U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" |\r\n");
    clboot_draw_box_line(L"independent UEFI loader for CLKS", L"x86_64 ISO", 76U);
    clboot_draw_box_line(L"protocol v2 / bootlog / graphical handoff", L"UEFI text UI", 76U);
    clboot_draw_box_bottom(76U);
    if (mode != (CHAR16 *)0) {
        clboot_print(L"  ");
        clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
        clboot_print(L"> ");
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        clboot_print(mode);
        clboot_print(L"\r\n");
    }
    clboot_print(L"\r\n");
}

static void clboot_draw_footer(CHAR16 *text) {
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L"  ");
    clboot_print(text);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_compact_header(CHAR16 *subtitle) {
    clboot_clear_screen();
    clboot_set_cursor(0U, 0U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_TITLE);
    clboot_print(L" CLeonOS CLBoot UEFI");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L"  x86_64 ISO / protocol v2");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
    clboot_print(L" ");
    clboot_print(subtitle);
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n");
}

static void clboot_halt(CHAR16 *message) {
    clboot_draw_brand_header(L"Boot halted");
    clboot_draw_box_top(76U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  | ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ERROR);
    clboot_print_padded(L"ERROR", 74U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" |\r\n");
    clboot_draw_box_line(message, L"", 76U);
    clboot_draw_box_line(L"Check the CLBoot log above or rebuild with verbose boot enabled.", L"", 76U);
    clboot_draw_box_bottom(76U);
    clboot_draw_footer(L"The machine is halted. Power off or reset to retry.");
    clboot_log(message);
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void clboot_draw_progress(UINTN percent, CHAR16 *label) {
    UINTN i;
    UINTN filled = percent / 4U;
    if (filled > 25U) {
        filled = 25U;
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"[");
    for (i = 0U; i < 25U; i++) {
        if (i < filled) {
            clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
            clboot_print(L"#");
        } else {
            clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
            clboot_print(L"-");
        }
    }
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"] ");
    if (percent < 100U) {
        clboot_print(L" ");
    }
    if (percent < 10U) {
        clboot_print(L" ");
    }
    clboot_print_uint(percent);
    clboot_print(L"%");
    clboot_print(L"  ");
    clboot_print(label);
    clboot_print(L"\r\n");
}

static EFI_STATUS clboot_read_key(EFI_INPUT_KEY *key) {
    if (clboot_st == (EFI_SYSTEM_TABLE *)0 || clboot_st->ConIn == (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *)0 ||
        clboot_st->ConIn->ReadKeyStroke == (EFI_INPUT_READ_KEY)0) {
        return EFI_NOT_FOUND;
    }
    return clboot_st->ConIn->ReadKeyStroke(clboot_st->ConIn, key);
}

static int clboot_poll_key(EFI_INPUT_KEY *key) {
    return (clboot_read_key(key) == EFI_SUCCESS) ? 1 : 0;
}

static void clboot_reset_input(void) {
    EFI_INPUT_KEY key;
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConIn != (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *)0 &&
        clboot_st->ConIn->Reset != (EFI_INPUT_RESET)0) {
        (void)clboot_st->ConIn->Reset(clboot_st->ConIn, 0U);
    }
    while (clboot_poll_key(&key) != 0) {
    }
}

static int clboot_alloc_pool(UINTN size, void **out) {
    if (clboot_bs->AllocatePool(EFI_LOADER_DATA, size, out) != EFI_SUCCESS) {
        return 0;
    }
    clboot_memset(*out, 0, size);
    return 1;
}

static int clboot_alloc_pages(UINTN pages, UINT64 *out_addr) {
    UINT64 addr = 0ULL;
    if (clboot_bs->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, pages, &addr) != EFI_SUCCESS) {
        return 0;
    }
    clboot_memset((void *)(UINTN)addr, 0, pages << 12U);
    *out_addr = addr;
    return 1;
}

static int clboot_alloc_pages_below(UINTN pages, UINT64 max_addr, UINT64 *out_addr) {
    UINT64 addr = max_addr;
    if (clboot_bs->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, pages, &addr) != EFI_SUCCESS) {
        return 0;
    }
    clboot_memset((void *)(UINTN)addr, 0, pages << 12U);
    *out_addr = addr;
    return 1;
}

static EFI_FILE_PROTOCOL *clboot_open_root(void) {
    EFI_LOADED_IMAGE_PROTOCOL *loaded = (EFI_LOADED_IMAGE_PROTOCOL *)0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0;
    EFI_FILE_PROTOCOL *root = (EFI_FILE_PROTOCOL *)0;

    if (clboot_bs->OpenProtocol(clboot_image, &gEfiLoadedImageProtocolGuid, (void **)&loaded, clboot_image,
                                (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS) {
        return (EFI_FILE_PROTOCOL *)0;
    }

    if (clboot_bs->OpenProtocol(loaded->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&fs, clboot_image,
                                (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS) {
        return (EFI_FILE_PROTOCOL *)0;
    }

    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS) {
        return (EFI_FILE_PROTOCOL *)0;
    }

    return root;
}

static int clboot_read_file_from_root(EFI_FILE_PROTOCOL *root, CHAR16 *path, struct clboot_loaded_file *out) {
    EFI_FILE_PROTOCOL *file;
    UINTN cap = 4096U;
    UINTN used = 0U;
    UINT8 *buf;

    out->data = (void *)0;
    out->size = 0ULL;
    if (root == (EFI_FILE_PROTOCOL *)0) {
        return 0;
    }
    if (root->Open(root, &file, path, EFI_FILE_MODE_READ, 0ULL) != EFI_SUCCESS) {
        return 0;
    }
    if (clboot_alloc_pool(cap, (void **)&buf) == 0) {
        (void)file->Close(file);
        return 0;
    }

    for (;;) {
        UINTN got = 4096U;
        if (used + got > cap) {
            UINT8 *new_buf;
            UINTN new_cap = cap * 2U;
            if (clboot_alloc_pool(new_cap, (void **)&new_buf) == 0) {
                (void)file->Close(file);
                return 0;
            }
            clboot_memcpy(new_buf, buf, used);
            (void)clboot_bs->FreePool(buf);
            buf = new_buf;
            cap = new_cap;
        }
        if (file->Read(file, &got, buf + used) != EFI_SUCCESS) {
            (void)file->Close(file);
            return 0;
        }
        if (got == 0U) {
            break;
        }
        used += got;
    }

    (void)file->Close(file);
    out->data = buf;
    out->size = (uint64_t)used;
    return 1;
}

static int clboot_read_file(CHAR16 *path, struct clboot_loaded_file *out) {
    EFI_FILE_PROTOCOL *root;
    EFI_HANDLE *handles;
    UINTN handle_count;
    UINTN i;

    root = clboot_open_root();
    if (root != (EFI_FILE_PROTOCOL *)0) {
        if (clboot_read_file_from_root(root, path, out) != 0) {
            (void)root->Close(root);
            return 1;
        }
        (void)root->Close(root);
    }

    handles = (EFI_HANDLE *)0;
    handle_count = 0U;
    if (clboot_bs->LocateHandleBuffer(EFI_BY_PROTOCOL, &gEfiSimpleFileSystemProtocolGuid, (void *)0, &handle_count,
                                      &handles) != EFI_SUCCESS ||
        handles == (EFI_HANDLE *)0) {
        return 0;
    }

    for (i = 0U; i < handle_count; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0;
        EFI_FILE_PROTOCOL *scan_root = (EFI_FILE_PROTOCOL *)0;
        if (clboot_bs->OpenProtocol(handles[i], &gEfiSimpleFileSystemProtocolGuid, (void **)&fs, clboot_image,
                                    (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS ||
            fs == (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0) {
            continue;
        }
        if (fs->OpenVolume(fs, &scan_root) != EFI_SUCCESS || scan_root == (EFI_FILE_PROTOCOL *)0) {
            continue;
        }
        if (clboot_read_file_from_root(scan_root, path, out) != 0) {
            (void)scan_root->Close(scan_root);
            (void)clboot_bs->FreePool(handles);
            return 1;
        }
        (void)scan_root->Close(scan_root);
    }

    (void)clboot_bs->FreePool(handles);
    return 0;
}

static int clboot_read_file_any(CHAR16 **paths, struct clboot_loaded_file *out) {
    UINTN i = 0U;
    while (paths[i] != (CHAR16 *)0) {
        if (clboot_read_file(paths[i], out) != 0) {
            return 1;
        }
        i++;
    }
    return 0;
}

static void clboot_connect_all_controllers(void) {
    EFI_HANDLE *handles;
    UINTN handle_count;
    UINTN i;

    handles = (EFI_HANDLE *)0;
    handle_count = 0U;
    if (clboot_bs->LocateHandleBuffer(EFI_ALL_HANDLES, (EFI_GUID *)0, (void *)0, &handle_count, &handles) !=
            EFI_SUCCESS ||
        handles == (EFI_HANDLE *)0) {
        return;
    }

    for (i = 0U; i < handle_count; i++) {
        (void)clboot_bs->ConnectController(handles[i], (EFI_HANDLE *)0, (void *)0, 1U);
    }

    (void)clboot_bs->FreePool(handles);
}

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

static void clboot_menu_entry_init(struct clboot_menu_entry *entry, const char *title, const char *hint,
                                   const char *kernel_path, const char *ramdisk_path, const char *cmdline,
                                   const char *source) {
    if (entry == (struct clboot_menu_entry *)0) {
        return;
    }
    clboot_ascii_to_wide(entry->title, CLBOOT_MENU_TITLE_MAX, title);
    clboot_ascii_to_wide(entry->hint, CLBOOT_MENU_HINT_MAX, hint);
    clboot_ascii_to_wide(entry->kernel_path, CLBOOT_MENU_PATH_MAX, kernel_path);
    clboot_ascii_to_wide(entry->ramdisk_path, CLBOOT_MENU_PATH_MAX, ramdisk_path);
    clboot_ascii_to_wide(entry->source, CLBOOT_MENU_HINT_MAX, source);
    clboot_copy_ascii(entry->cmdline, CLBOOT_MENU_CMDLINE_MAX, cmdline);
}

static struct clboot_menu_entry *clboot_menu_add(struct clboot_menu_config *cfg, const char *title, const char *hint,
                                                 const char *kernel_path, const char *ramdisk_path,
                                                 const char *cmdline, const char *source) {
    struct clboot_menu_entry *entry;

    if (cfg == (struct clboot_menu_config *)0 || cfg->count >= CLBOOT_MENU_MAX_ENTRIES) {
        return (struct clboot_menu_entry *)0;
    }

    entry = &cfg->entries[cfg->count++];
    clboot_menu_entry_init(entry, title, hint, kernel_path, ramdisk_path, cmdline, source);
    return entry;
}

static void clboot_menu_config_init_defaults(struct clboot_menu_config *cfg) {
    if (cfg == (struct clboot_menu_config *)0) {
        return;
    }

    clboot_memset(cfg, 0, sizeof(*cfg));
    cfg->timeout_seconds = CLBOOT_MENU_TIMEOUT_SECONDS;
    clboot_copy_ascii(cfg->global_cmdline, sizeof(cfg->global_cmdline), CLBOOT_DEFAULT_CMDLINE);

    (void)clboot_menu_add(cfg, "Try CLeonOS", "Normal ISO live boot.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "", "builtin");
    (void)clboot_menu_add(cfg, "Safe Mode", "No splash, root rescue mode, verbose logs.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "clks.rescue=1 clks.nosplash clks.loglevel=debug",
                          "builtin");
    (void)clboot_menu_add(cfg, "Verbose Boot", "Normal boot with debug-level kernel logs.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "clks.loglevel=debug", "builtin");
    (void)clboot_menu_add(cfg, "Quiet Boot", "Normal boot with quiet kernel logging.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "clks.loglevel=quiet", "builtin");
    (void)clboot_menu_add(cfg, "Install to Disk", "Boot and auto-run install2disk install.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=install clks.nosplash", "builtin");
    (void)clboot_menu_add(cfg, "Repair Disk System", "Boot and auto-run install2disk repair.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=repair clks.nosplash", "builtin");
    (void)clboot_menu_add(cfg, "Update Kernel", "Boot and auto-run install2disk update-kernel.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=update-kernel clks.nosplash", "builtin");
    (void)clboot_menu_add(cfg, "Verify Installation", "Boot and auto-run install2disk verify.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=verify clks.nosplash", "builtin");
}

static UINTN clboot_parse_uint(const char *text, UINTN len, UINTN fallback) {
    UINTN value = 0U;
    UINTN i;
    int any = 0;

    if (text == (const char *)0) {
        return fallback;
    }
    for (i = 0U; i < len && text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return fallback;
        }
        value = (value * 10U) + (UINTN)(text[i] - '0');
        any = 1;
    }
    return any != 0 ? value : fallback;
}

static void clboot_trim_line(const char *data, UINTN len, UINTN *out_start, UINTN *out_len) {
    UINTN start = 0U;
    UINTN end = len;

    while (start < len && (data[start] == ' ' || data[start] == '\t')) {
        start++;
    }
    while (end > start && (data[end - 1U] == ' ' || data[end - 1U] == '\t' || data[end - 1U] == '\r')) {
        end--;
    }
    *out_start = start;
    *out_len = end - start;
}

static int clboot_line_key_value(const char *line, UINTN line_len, const char **out_key, UINTN *out_key_len,
                                 const char **out_value, UINTN *out_value_len) {
    UINTN eq = 0U;
    UINTN key_start;
    UINTN key_len;
    UINTN value_start;
    UINTN value_len;

    while (eq < line_len && line[eq] != '=') {
        eq++;
    }
    if (eq == line_len) {
        return 0;
    }

    clboot_trim_line(line, eq, &key_start, &key_len);
    clboot_trim_line(line + eq + 1U, line_len - eq - 1U, &value_start, &value_len);
    *out_key = line + key_start;
    *out_key_len = key_len;
    *out_value = line + eq + 1U + value_start;
    *out_value_len = value_len;
    return 1;
}

static void clboot_menu_parse_config(struct clboot_menu_config *cfg) {
    struct clboot_loaded_file config;
    char *data;
    uint64_t pos;
    struct clboot_menu_entry *current = (struct clboot_menu_entry *)0;
    UINTN old_count;
    CHAR16 *config_paths[] = {
        CLBOOT_CONFIG_PATH,
        L"\\EFI\\CLEONOS\\CLBOOT.CONF",
        L"\\EFI\\CLEONOS\\CLBOOT~1.CON",
        (CHAR16 *)0,
    };

    if (cfg == (struct clboot_menu_config *)0) {
        return;
    }

    old_count = cfg->count;
    if (clboot_read_file_any(config_paths, &config) == 0 || config.size == 0ULL) {
        return;
    }

    data = (char *)config.data;
    for (pos = 0ULL; pos < config.size;) {
        uint64_t line_start = pos;
        UINTN raw_len;
        UINTN trim_start;
        UINTN trim_len;
        const char *line;
        const char *key;
        const char *value;
        UINTN key_len;
        UINTN value_len;

        while (pos < config.size && data[pos] != '\n') {
            pos++;
        }
        raw_len = (UINTN)(pos - line_start);
        if (pos < config.size && data[pos] == '\n') {
            pos++;
        }

        clboot_trim_line(data + line_start, raw_len, &trim_start, &trim_len);
        if (trim_len == 0U) {
            continue;
        }
        line = data + line_start + trim_start;
        if (line[0] == '#') {
            continue;
        }
        if (clboot_line_key_value(line, trim_len, &key, &key_len, &value, &value_len) == 0) {
            continue;
        }

        if (clboot_str_equal_n(key, "timeout", key_len) != 0) {
            cfg->timeout_seconds = clboot_parse_uint(value, value_len, cfg->timeout_seconds);
        } else if (clboot_str_equal_n(key, "default", key_len) != 0) {
            cfg->default_index = clboot_parse_uint(value, value_len, cfg->default_index);
        } else if (clboot_str_equal_n(key, "cmdline", key_len) != 0 && current == (struct clboot_menu_entry *)0) {
            clboot_copy_ascii_n(cfg->global_cmdline, sizeof(cfg->global_cmdline), value, value_len);
        } else if (clboot_str_equal_n(key, "menuentry", key_len) != 0) {
            if (cfg->count == old_count) {
                cfg->count = 0U;
            }
            current = clboot_menu_add(cfg, "", "Configured boot entry.", "\\boot\\clks_kernel.elf",
                                      "\\boot\\cleonos_ramdisk.tar", "", "clboot.conf");
            if (current != (struct clboot_menu_entry *)0) {
                clboot_ascii_n_to_wide(current->title, CLBOOT_MENU_TITLE_MAX, value, value_len);
            }
        } else if (current != (struct clboot_menu_entry *)0) {
            if (clboot_str_equal_n(key, "kernel", key_len) != 0) {
                clboot_ascii_n_to_wide(current->kernel_path, CLBOOT_MENU_PATH_MAX, value, value_len);
            } else if (clboot_str_equal_n(key, "ramdisk", key_len) != 0) {
                clboot_ascii_n_to_wide(current->ramdisk_path, CLBOOT_MENU_PATH_MAX, value, value_len);
            } else if (clboot_str_equal_n(key, "cmdline", key_len) != 0) {
                clboot_copy_ascii_n(current->cmdline, CLBOOT_MENU_CMDLINE_MAX, value, value_len);
            } else if (clboot_str_equal_n(key, "hint", key_len) != 0) {
                clboot_ascii_n_to_wide(current->hint, CLBOOT_MENU_HINT_MAX, value, value_len);
            }
        }
    }

    if (cfg->count == 0U) {
        clboot_menu_config_init_defaults(cfg);
    }
    if (cfg->default_index >= cfg->count) {
        cfg->default_index = 0U;
    }
}

static int clboot_wide_ends_with_elf(CHAR16 *name) {
    UINTN len = clboot_wstrlen(name);
    CHAR16 a;
    CHAR16 b;
    CHAR16 c;
    CHAR16 d;

    if (len < 4U) {
        return 0;
    }
    a = name[len - 4U];
    b = name[len - 3U];
    c = name[len - 2U];
    d = name[len - 1U];
    if (b >= L'A' && b <= L'Z') {
        b = (CHAR16)(b - L'A' + L'a');
    }
    if (c >= L'A' && c <= L'Z') {
        c = (CHAR16)(c - L'A' + L'a');
    }
    if (d >= L'A' && d <= L'Z') {
        d = (CHAR16)(d - L'A' + L'a');
    }
    return (a == L'.' && b == L'e' && c == L'l' && d == L'f') ? 1 : 0;
}

static void clboot_wide_path_join(CHAR16 *dst, UINTN dst_size, CHAR16 *dir, CHAR16 *name) {
    UINTN pos = 0U;
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    while (pos + 1U < dst_size && dir != (CHAR16 *)0 && dir[i] != 0U) {
        dst[pos++] = dir[i++];
    }
    if (pos > 0U && dst[pos - 1U] != L'\\' && pos + 1U < dst_size) {
        dst[pos++] = L'\\';
    }
    i = 0U;
    while (pos + 1U < dst_size && name != (CHAR16 *)0 && name[i] != 0U) {
        dst[pos++] = name[i++];
    }
    dst[pos] = 0U;
}

static void clboot_menu_scan_kernel_dir(struct clboot_menu_config *cfg) {
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *dir;
    UINT8 info_buf[512];
    static CHAR16 kernel_dir[] = L"\\boot\\kernels";

    if (cfg == (struct clboot_menu_config *)0 || cfg->count >= CLBOOT_MENU_MAX_ENTRIES) {
        return;
    }

    root = clboot_open_root();
    if (root == (EFI_FILE_PROTOCOL *)0) {
        return;
    }
    if (root->Open(root, &dir, kernel_dir, EFI_FILE_MODE_READ, 0ULL) != EFI_SUCCESS) {
        (void)root->Close(root);
        return;
    }

    for (;;) {
        EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_buf;
        UINTN got = sizeof(info_buf);
        struct clboot_menu_entry *entry;
        CHAR16 kernel_path[CLBOOT_MENU_PATH_MAX];

        if (dir->Read(dir, &got, info_buf) != EFI_SUCCESS || got == 0U) {
            break;
        }
        if ((info->Attribute & EFI_FILE_DIRECTORY) != 0ULL) {
            continue;
        }
        if (clboot_wide_ends_with_elf(info->FileName) == 0) {
            continue;
        }
        if (cfg->count >= CLBOOT_MENU_MAX_ENTRIES) {
            break;
        }

        clboot_wide_path_join(kernel_path, CLBOOT_MENU_PATH_MAX, kernel_dir, info->FileName);
        entry = &cfg->entries[cfg->count++];
        clboot_copy_wide(entry->title, CLBOOT_MENU_TITLE_MAX, info->FileName);
        clboot_copy_wide(entry->hint, CLBOOT_MENU_HINT_MAX, L"Kernel from /boot/kernels.");
        clboot_copy_wide(entry->kernel_path, CLBOOT_MENU_PATH_MAX, kernel_path);
        clboot_ascii_to_wide(entry->ramdisk_path, CLBOOT_MENU_PATH_MAX, "\\boot\\cleonos_ramdisk.tar");
        clboot_copy_wide(entry->source, CLBOOT_MENU_HINT_MAX, L"/boot/kernels scan");
        clboot_copy_ascii(entry->cmdline, CLBOOT_MENU_CMDLINE_MAX, "");
    }

    (void)dir->Close(dir);
    (void)root->Close(root);
}

static char *clboot_cmdline_build(const char *base, const char *extra) {
    char *cmdline;
    UINTN base_len = clboot_strlen(base);
    UINTN extra_len = clboot_strlen(extra);
    UINTN need_space = (base_len > 0U && extra_len > 0U) ? 1U : 0U;

    if (clboot_alloc_pool(base_len + need_space + extra_len + 1U, (void **)&cmdline) == 0) {
        return (char *)0;
    }
    if (base_len > 0U) {
        clboot_memcpy(cmdline, base, base_len);
    }
    if (need_space != 0U) {
        cmdline[base_len] = ' ';
    }
    if (extra_len > 0U) {
        clboot_memcpy(cmdline + base_len + need_space, extra, extra_len);
    }
    cmdline[base_len + need_space + extra_len] = '\0';
    return cmdline;
}

static void clboot_draw_cmdline_editor(struct clboot_menu_entry *entry, UINTN cursor) {
    clboot_draw_compact_header(L"Edit boot arguments");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Entry: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->title);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Kernel: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->kernel_path);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L" ");
    clboot_print(L"cmdline: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_WARN);
    clboot_print((CHAR16 *)L"(entry arguments only)");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n ");
    {
        UINTN i = 0U;
        while (entry->cmdline[i] != '\0') {
            CHAR16 ch[2];
            if (i == cursor) {
                clboot_set_attr(CLBOOT_TEXT_ATTR_SELECTED);
            }
            ch[0] = (CHAR16)(unsigned char)entry->cmdline[i];
            ch[1] = 0U;
            clboot_print(ch);
            if (i == cursor) {
                clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
            }
            i++;
        }
        if (cursor == i) {
            clboot_set_attr(CLBOOT_TEXT_ATTR_SELECTED);
            clboot_print(L" ");
            clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        }
    }
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Type to edit   Backspace: delete   Enter: save   Esc: cancel\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_edit_cmdline(struct clboot_menu_entry *entry) {
    char original[CLBOOT_MENU_CMDLINE_MAX];
    UINTN cursor;
    EFI_INPUT_KEY key;

    if (entry == (struct clboot_menu_entry *)0) {
        return;
    }
    clboot_copy_ascii(original, sizeof(original), entry->cmdline);
    cursor = clboot_strlen(entry->cmdline);
    clboot_draw_cmdline_editor(entry, cursor);

    for (;;) {
        if (clboot_poll_key(&key) == 0) {
            if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
                (void)clboot_bs->Stall(20000U);
            }
            continue;
        }
        if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n') {
            return;
        }
        if (key.ScanCode == 23U) {
            clboot_copy_ascii(entry->cmdline, sizeof(entry->cmdline), original);
            return;
        }
        if (key.UnicodeChar == 8U) {
            if (cursor > 0U) {
                UINTN len = clboot_strlen(entry->cmdline);
                UINTN i;
                for (i = cursor - 1U; i < len; i++) {
                    entry->cmdline[i] = entry->cmdline[i + 1U];
                }
                cursor--;
            }
        } else if (key.UnicodeChar >= 32U && key.UnicodeChar < 127U) {
            UINTN len = clboot_strlen(entry->cmdline);
            if (len + 1U < sizeof(entry->cmdline)) {
                UINTN i;
                for (i = len + 1U; i > cursor; i--) {
                    entry->cmdline[i] = entry->cmdline[i - 1U];
                }
                entry->cmdline[cursor] = (char)key.UnicodeChar;
                cursor++;
            }
        } else if (key.ScanCode == 3U) {
            if (cursor > 0U) {
                cursor--;
            }
        } else if (key.ScanCode == 4U) {
            if (cursor < clboot_strlen(entry->cmdline)) {
                cursor++;
            }
        }
        clboot_draw_cmdline_editor(entry, cursor);
    }
}

static void clboot_wait_any_key(void) {
    EFI_INPUT_KEY key;

    clboot_reset_input();
    for (;;) {
        if (clboot_poll_key(&key) != 0) {
            return;
        }
        if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
            (void)clboot_bs->Stall(20000U);
        }
    }
}

static void clboot_print_ascii_wrapped(const char *text, UINTN width) {
    UINTN col = 0U;
    UINTN i = 0U;

    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        CHAR16 ch[2];
        if (col >= width && text[i] == ' ') {
            clboot_print(L"\r\n ");
            col = 0U;
            i++;
            continue;
        }
        if (col >= width) {
            clboot_print(L"\r\n ");
            col = 0U;
        }
        ch[0] = (CHAR16)(unsigned char)text[i++];
        ch[1] = 0U;
        clboot_print(ch);
        col++;
    }
}

static void clboot_show_cmdline_preview(const struct clboot_menu_config *cfg, const struct clboot_menu_entry *entry) {
    char *final_cmdline;

    final_cmdline = clboot_cmdline_build(cfg->global_cmdline, entry->cmdline);
    clboot_draw_compact_header(L"Final command line preview");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Entry: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->title);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Formula: global cmdline + entry cmdline\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L" ");
    clboot_print_ascii_wrapped(final_cmdline == (char *)0 ? "<allocation failed>" : final_cmdline, 76U);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Press any key to return to Boot Menu.\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_wait_any_key();
}

static void clboot_show_entry_info(const struct clboot_menu_entry *entry) {
    clboot_draw_compact_header(L"Boot entry details");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Title   : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->title);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Kernel  : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->kernel_path);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Ramdisk : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->ramdisk_path);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Source  : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->source);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Hint    : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->hint);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Cmdline : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print_ascii_wrapped(entry->cmdline[0] == '\0' ? "<empty>" : entry->cmdline, 66U);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Press any key to return to Boot Menu.\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_wait_any_key();
}

static void clboot_draw_menu(const struct clboot_menu_entry *entries, UINTN count, UINTN selected,
                             UINTN seconds_left, int countdown_active) {
    UINTN i;
    UINTN page_size = 8U;
    UINTN page = selected / page_size;
    UINTN start = page * page_size;
    UINTN end = start + page_size;
    if (end > count) {
        end = count;
    }

    clboot_draw_compact_header(L"Boot menu");
    clboot_print(L" ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_WARN);
    if (countdown_active != 0) {
        clboot_print(L"Auto boot: ");
        clboot_print_uint(seconds_left);
        clboot_print(L"s   ");
    } else {
        clboot_print(L"Auto boot: stopped   ");
    }
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"Enter: boot   e: edit   c: cmdline   i: info   +/-: page\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);

    for (i = start; i < end; i++) {
        CHAR16 prefix[8];
        prefix[0] = L' ';
        prefix[1] = (i == selected) ? L'>' : L' ';
        prefix[2] = L' ';
        prefix[3] = (CHAR16)(L'1' + (i - start));
        prefix[4] = L'.';
        prefix[5] = L' ';
        prefix[6] = 0U;

        clboot_set_attr((i == selected) ? CLBOOT_TEXT_ATTR_SELECTED : CLBOOT_TEXT_ATTR_NORMAL);
        clboot_print(prefix);
        clboot_print_padded(entries[i].title, 72U);
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        clboot_print(L"\r\n");
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" ");
    clboot_print(entries[selected].hint);
    clboot_print(L"\r\n Page ");
    clboot_print_uint(page + 1U);
    clboot_print(L"/");
    clboot_print_uint((count + page_size - 1U) / page_size);
    clboot_print(L"   Bootlog will be passed to CLKS through protocol v2.\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static UINTN clboot_boot_menu_select(struct clboot_menu_config *cfg) {
    UINTN count = cfg->count;
    UINTN selected = cfg->default_index;
    UINTN seconds_left = cfg->timeout_seconds;
    int countdown_active = (seconds_left > 0U) ? 1 : 0;
    UINTN ticks;
    EFI_INPUT_KEY key;

    clboot_reset_input();
    clboot_enable_cursor(0);
    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);

    for (;;) {
        for (ticks = 0U; ticks < 10U; ticks++) {
            if (clboot_poll_key(&key) != 0) {
                if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n') {
                    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
                    return selected;
                }
                if ((key.UnicodeChar == L'e' || key.UnicodeChar == L'E') && selected < count) {
                    countdown_active = 0;
                    clboot_edit_cmdline(&cfg->entries[selected]);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if ((key.UnicodeChar == L'c' || key.UnicodeChar == L'C') && selected < count) {
                    countdown_active = 0;
                    clboot_show_cmdline_preview(cfg, &cfg->entries[selected]);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if ((key.UnicodeChar == L'i' || key.UnicodeChar == L'I') && selected < count) {
                    countdown_active = 0;
                    clboot_show_entry_info(&cfg->entries[selected]);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.UnicodeChar >= L'1' && key.UnicodeChar <= L'8') {
                    UINTN page_start = (selected / 8U) * 8U;
                    UINTN next = page_start + (UINTN)(key.UnicodeChar - L'1');
                    if (next >= count) {
                        continue;
                    }
                    countdown_active = 0;
                    selected = next;
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.UnicodeChar == L'+' || key.UnicodeChar == L'=') {
                    UINTN next = ((selected / 8U) + 1U) * 8U;
                    countdown_active = 0;
                    if (next < count) {
                        selected = next;
                    }
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.UnicodeChar == L'-' || key.UnicodeChar == L'_') {
                    UINTN page = selected / 8U;
                    countdown_active = 0;
                    if (page > 0U) {
                        selected = (page - 1U) * 8U;
                    }
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.ScanCode == 1U) {
                    countdown_active = 0;
                    selected = (selected == 0U) ? (count - 1U) : (selected - 1U);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.ScanCode == 2U) {
                    countdown_active = 0;
                    selected = (selected + 1U) % count;
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
            }
            if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
                (void)clboot_bs->Stall(100000U);
            }
        }
        if (countdown_active != 0) {
            if (seconds_left == 0U) {
                break;
            }
            seconds_left--;
            clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
            if (seconds_left == 0U) {
                break;
            }
        }
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    return selected;
}

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

    clboot_select_text_mode();
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_clear_screen();
    clboot_log_append_ascii("CLBoot UEFI starting\n");

    menu_config = &clboot_menu_config_storage;
    clboot_menu_config_init_defaults(menu_config);
    clboot_menu_parse_config(menu_config);
    clboot_menu_scan_kernel_dir(menu_config);
    menu_index = clboot_boot_menu_select(menu_config);
    boot_entry = &menu_config->entries[menu_index];
    clboot_draw_compact_header(L"Boot progress");
    clboot_print(L" Preparing kernel handoff.\r\n");
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
    if (clboot_read_file(boot_entry->ramdisk_path, &ramdisk_file) == 0) {
        clboot_halt(L"CLBoot: cannot read ramdisk");
    }
    clboot_status(L"ramdisk loaded");

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
        clboot_alloc_pool(sizeof(*module), (void **)&module) == 0) {
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

    module->address = (uint64_t)(UINTN)ramdisk_file.data;
    module->size = ramdisk_file.size;
    module->path = (uint64_t)(UINTN)"ramdisk";
    module->cmdline = (uint64_t)(UINTN)"ramdisk";

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
    info->modules = (uint64_t)(UINTN)module;
    info->module_count = 1ULL;
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
