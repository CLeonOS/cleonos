#include <clks/disk.h>
#include <clks/heap.h>
#include <clks/log.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_DISK_MIN_BYTES (4ULL * 1024ULL * 1024ULL)
#define CLKS_DISK_CACHE_MAX_BYTES (8ULL * 1024ULL * 1024ULL)

#define CLKS_DISK_MAX_NODES 256U
#define CLKS_DISK_NODE_FLAG_HEAP_DATA 0x0001U

#define CLKS_DISK_META_OFFSET 0x00010000ULL
#define CLKS_DISK_META_SIZE 0x00020000ULL
#define CLKS_DISK_META_HEADER_SIZE 32U
#define CLKS_DISK_META_ENTRY_SIZE 224U
#define CLKS_DISK_DATA_OFFSET (CLKS_DISK_META_OFFSET + CLKS_DISK_META_SIZE)

#define CLKS_DISK_META_MAGIC "CLDSKFS1"
#define CLKS_DISK_META_VERSION 1U

#define CLKS_DISK_FAT32_BOOT_SIG_OFFSET 510U
#define CLKS_DISK_FAT32_TYPE_OFFSET 82U
#define CLKS_DISK_FAT32_TYPE_LEN 5U

#if defined(CLKS_ARCH_X86_64)
#define CLKS_DISK_ATA_IO_BASE 0x1F0U
#define CLKS_DISK_ATA_CTRL_BASE 0x3F6U
#define CLKS_DISK_ATA_REG_DATA (CLKS_DISK_ATA_IO_BASE + 0U)
#define CLKS_DISK_ATA_REG_SECTOR_COUNT (CLKS_DISK_ATA_IO_BASE + 2U)
#define CLKS_DISK_ATA_REG_LBA_LOW (CLKS_DISK_ATA_IO_BASE + 3U)
#define CLKS_DISK_ATA_REG_LBA_MID (CLKS_DISK_ATA_IO_BASE + 4U)
#define CLKS_DISK_ATA_REG_LBA_HIGH (CLKS_DISK_ATA_IO_BASE + 5U)
#define CLKS_DISK_ATA_REG_DRIVE (CLKS_DISK_ATA_IO_BASE + 6U)
#define CLKS_DISK_ATA_REG_STATUS (CLKS_DISK_ATA_IO_BASE + 7U)
#define CLKS_DISK_ATA_REG_COMMAND (CLKS_DISK_ATA_IO_BASE + 7U)
#define CLKS_DISK_ATA_REG_ALT_STATUS (CLKS_DISK_ATA_CTRL_BASE + 0U)

#define CLKS_DISK_ATA_STATUS_ERR 0x01U
#define CLKS_DISK_ATA_STATUS_DRQ 0x08U
#define CLKS_DISK_ATA_STATUS_DF 0x20U
#define CLKS_DISK_ATA_STATUS_DRDY 0x40U
#define CLKS_DISK_ATA_STATUS_BSY 0x80U

#define CLKS_DISK_ATA_CMD_READ_SECTORS 0x20U
#define CLKS_DISK_ATA_CMD_WRITE_SECTORS 0x30U
#define CLKS_DISK_ATA_CMD_CACHE_FLUSH 0xE7U
#define CLKS_DISK_ATA_CMD_IDENTIFY 0xECU
#endif

struct clks_disk_node {
    clks_bool used;
    u8 type;
    u16 parent;
    u16 flags;
    const void *data;
    u64 size;
    char path[CLKS_DISK_PATH_MAX];
};

static u8 *clks_disk_bytes = CLKS_NULL;
static u64 clks_disk_bytes_len = 0ULL;
static u64 clks_disk_sector_total = 0ULL;
static clks_bool clks_disk_ready = CLKS_FALSE;
static clks_bool clks_disk_formatted = CLKS_FALSE;
static clks_bool clks_disk_mounted = CLKS_FALSE;
static clks_bool clks_disk_hw_backed = CLKS_FALSE;
static char clks_disk_mount_path_buf[CLKS_DISK_PATH_MAX];
static const u8 clks_disk_empty_file_data[1] = {0U};

static struct clks_disk_node clks_disk_nodes[CLKS_DISK_MAX_NODES];
static u16 clks_disk_nodes_used = 0U;

static u16 clks_disk_read_u16(const u8 *ptr) {
    return (u16)((u16)ptr[0] | ((u16)ptr[1] << 8));
}

static u32 clks_disk_read_u32(const u8 *ptr) {
    return (u32)((u32)ptr[0] | ((u32)ptr[1] << 8) | ((u32)ptr[2] << 16) | ((u32)ptr[3] << 24));
}

static u64 clks_disk_read_u64(const u8 *ptr) {
    u64 value = 0ULL;
    u32 i;

    for (i = 0U; i < 8U; i++) {
        value |= ((u64)ptr[i]) << (u64)(i * 8U);
    }

    return value;
}

static void clks_disk_write_u16(u8 *ptr, u16 value) {
    ptr[0] = (u8)(value & 0xFFU);
    ptr[1] = (u8)((value >> 8) & 0xFFU);
}

static void clks_disk_write_u32(u8 *ptr, u32 value) {
    ptr[0] = (u8)(value & 0xFFU);
    ptr[1] = (u8)((value >> 8) & 0xFFU);
    ptr[2] = (u8)((value >> 16) & 0xFFU);
    ptr[3] = (u8)((value >> 24) & 0xFFU);
}

static void clks_disk_write_u64(u8 *ptr, u64 value) {
    u32 i;

    for (i = 0U; i < 8U; i++) {
        ptr[i] = (u8)((value >> (u64)(i * 8U)) & 0xFFULL);
    }
}

#if defined(CLKS_ARCH_X86_64)
static inline void clks_disk_ata_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 clks_disk_ata_inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void clks_disk_ata_outw(u16 port, u16 value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline u16 clks_disk_ata_inw(u16 port) {
    u16 value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void clks_disk_ata_delay_400ns(void) {
    (void)clks_disk_ata_inb(CLKS_DISK_ATA_REG_ALT_STATUS);
    (void)clks_disk_ata_inb(CLKS_DISK_ATA_REG_ALT_STATUS);
    (void)clks_disk_ata_inb(CLKS_DISK_ATA_REG_ALT_STATUS);
    (void)clks_disk_ata_inb(CLKS_DISK_ATA_REG_ALT_STATUS);
}

static clks_bool clks_disk_ata_wait_not_busy(void) {
    u32 i;

    for (i = 0U; i < 1000000U; i++) {
        u8 status = clks_disk_ata_inb(CLKS_DISK_ATA_REG_STATUS);

        if ((status & CLKS_DISK_ATA_STATUS_BSY) == 0U) {
            return CLKS_TRUE;
        }
    }

    return CLKS_FALSE;
}

static clks_bool clks_disk_ata_wait_drq_ready(void) {
    u32 i;

    for (i = 0U; i < 1000000U; i++) {
        u8 status = clks_disk_ata_inb(CLKS_DISK_ATA_REG_STATUS);

        if ((status & CLKS_DISK_ATA_STATUS_BSY) != 0U) {
            continue;
        }

        if ((status & (CLKS_DISK_ATA_STATUS_ERR | CLKS_DISK_ATA_STATUS_DF)) != 0U) {
            return CLKS_FALSE;
        }

        if ((status & CLKS_DISK_ATA_STATUS_DRQ) != 0U) {
            return CLKS_TRUE;
        }
    }

    return CLKS_FALSE;
}

static clks_bool clks_disk_ata_wait_ready_no_drq(void) {
    u32 i;

    for (i = 0U; i < 1000000U; i++) {
        u8 status = clks_disk_ata_inb(CLKS_DISK_ATA_REG_STATUS);

        if ((status & CLKS_DISK_ATA_STATUS_BSY) != 0U) {
            continue;
        }

        if ((status & (CLKS_DISK_ATA_STATUS_ERR | CLKS_DISK_ATA_STATUS_DF)) != 0U) {
            return CLKS_FALSE;
        }

        if ((status & CLKS_DISK_ATA_STATUS_DRDY) != 0U) {
            return CLKS_TRUE;
        }
    }

    return CLKS_FALSE;
}

static clks_bool clks_disk_ata_identify(u64 *out_sector_total) {
    u16 identify_words[256];
    u32 i;
    u64 sector_total;
    u8 status;

    if (out_sector_total == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_disk_ata_wait_not_busy() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_disk_ata_outb(CLKS_DISK_ATA_REG_DRIVE, 0xA0U);
    clks_disk_ata_delay_400ns();

    clks_disk_ata_outb(CLKS_DISK_ATA_REG_SECTOR_COUNT, 0U);
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_LOW, 0U);
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_MID, 0U);
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_HIGH, 0U);
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_COMMAND, CLKS_DISK_ATA_CMD_IDENTIFY);

    status = clks_disk_ata_inb(CLKS_DISK_ATA_REG_STATUS);
    if (status == 0U) {
        return CLKS_FALSE;
    }

    if (clks_disk_ata_wait_drq_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < 256U; i++) {
        identify_words[i] = clks_disk_ata_inw(CLKS_DISK_ATA_REG_DATA);
    }

    sector_total = (u64)identify_words[60] | ((u64)identify_words[61] << 16U);
    if (sector_total == 0ULL) {
        return CLKS_FALSE;
    }

    *out_sector_total = sector_total;
    return CLKS_TRUE;
}

static clks_bool clks_disk_ata_read_sector_hw(u64 lba, void *out_sector) {
    u8 *out = (u8 *)out_sector;
    u32 i;
    u8 status;

    if (out_sector == CLKS_NULL || lba > 0x0FFFFFFFULL) {
        return CLKS_FALSE;
    }

    if (clks_disk_ata_wait_not_busy() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_disk_ata_outb(CLKS_DISK_ATA_REG_DRIVE, (u8)(0xE0U | ((u8)((lba >> 24U) & 0x0FU))));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_SECTOR_COUNT, 1U);
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_LOW, (u8)(lba & 0xFFULL));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_MID, (u8)((lba >> 8U) & 0xFFULL));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_HIGH, (u8)((lba >> 16U) & 0xFFULL));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_COMMAND, CLKS_DISK_ATA_CMD_READ_SECTORS);

    if (clks_disk_ata_wait_drq_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < 256U; i++) {
        u16 word = clks_disk_ata_inw(CLKS_DISK_ATA_REG_DATA);
        out[i * 2U] = (u8)(word & 0x00FFU);
        out[i * 2U + 1U] = (u8)((word >> 8U) & 0x00FFU);
    }

    clks_disk_ata_delay_400ns();
    status = clks_disk_ata_inb(CLKS_DISK_ATA_REG_STATUS);
    if ((status & (CLKS_DISK_ATA_STATUS_ERR | CLKS_DISK_ATA_STATUS_DF)) != 0U) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_ata_write_sector_hw(u64 lba, const void *sector_data) {
    const u8 *src = (const u8 *)sector_data;
    u32 i;

    if (sector_data == CLKS_NULL || lba > 0x0FFFFFFFULL) {
        return CLKS_FALSE;
    }

    if (clks_disk_ata_wait_not_busy() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_disk_ata_outb(CLKS_DISK_ATA_REG_DRIVE, (u8)(0xE0U | ((u8)((lba >> 24U) & 0x0FU))));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_SECTOR_COUNT, 1U);
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_LOW, (u8)(lba & 0xFFULL));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_MID, (u8)((lba >> 8U) & 0xFFULL));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_LBA_HIGH, (u8)((lba >> 16U) & 0xFFULL));
    clks_disk_ata_outb(CLKS_DISK_ATA_REG_COMMAND, CLKS_DISK_ATA_CMD_WRITE_SECTORS);

    if (clks_disk_ata_wait_drq_ready() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < 256U; i++) {
        u16 word = (u16)src[i * 2U] | ((u16)src[i * 2U + 1U] << 8U);
        clks_disk_ata_outw(CLKS_DISK_ATA_REG_DATA, word);
    }

    clks_disk_ata_delay_400ns();
    if (clks_disk_ata_wait_ready_no_drq() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_ata_cache_flush_hw(void) {
    if (clks_disk_ata_wait_not_busy() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_disk_ata_outb(CLKS_DISK_ATA_REG_COMMAND, CLKS_DISK_ATA_CMD_CACHE_FLUSH);
    return clks_disk_ata_wait_ready_no_drq();
}
#else
static clks_bool clks_disk_ata_identify(u64 *out_sector_total) {
    (void)out_sector_total;
    return CLKS_FALSE;
}

static clks_bool clks_disk_ata_read_sector_hw(u64 lba, void *out_sector) {
    (void)lba;
    (void)out_sector;
    return CLKS_FALSE;
}

static clks_bool clks_disk_ata_write_sector_hw(u64 lba, const void *sector_data) {
    (void)lba;
    (void)sector_data;
    return CLKS_FALSE;
}

static clks_bool clks_disk_ata_cache_flush_hw(void) {
    return CLKS_FALSE;
}
#endif

static clks_bool clks_disk_sync_bytes_to_hw(u64 used_bytes) {
    u64 sectors_to_sync;
    u64 lba;

    if (clks_disk_hw_backed == CLKS_FALSE) {
        return CLKS_TRUE;
    }

    if (clks_disk_bytes == CLKS_NULL || clks_disk_sector_total == 0ULL) {
        return CLKS_FALSE;
    }

    if (used_bytes == 0ULL) {
        return CLKS_TRUE;
    }

    if (used_bytes > clks_disk_bytes_len) {
        used_bytes = clks_disk_bytes_len;
    }

    sectors_to_sync = (used_bytes + (CLKS_DISK_SECTOR_SIZE - 1ULL)) / CLKS_DISK_SECTOR_SIZE;
    if (sectors_to_sync > clks_disk_sector_total) {
        sectors_to_sync = clks_disk_sector_total;
    }

    for (lba = 0ULL; lba < sectors_to_sync; lba++) {
        const u8 *sector_ptr = clks_disk_bytes + (usize)(lba * CLKS_DISK_SECTOR_SIZE);
        if (clks_disk_ata_write_sector_hw(lba, sector_ptr) == CLKS_FALSE) {
            return CLKS_FALSE;
        }
    }

    if (clks_disk_ata_cache_flush_hw() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_bytes_equal(const u8 *left, const u8 *right, usize count) {
    usize i;

    if (left == CLKS_NULL || right == CLKS_NULL) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < count; i++) {
        if (left[i] != right[i]) {
            return CLKS_FALSE;
        }
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_text_prefix_equals(const char *text, const char *prefix, usize prefix_len) {
    usize i;

    if (text == CLKS_NULL || prefix == CLKS_NULL) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < prefix_len; i++) {
        if (text[i] != prefix[i]) {
            return CLKS_FALSE;
        }
    }

    return CLKS_TRUE;
}

static void clks_disk_copy_text(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || dst_size == 0U) {
        return;
    }

    if (src == CLKS_NULL) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1U < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static clks_bool clks_disk_normalize_absolute_path(const char *path, char *out_path, usize out_size) {
    usize in_pos = 0U;
    usize out_pos = 0U;

    if (path == CLKS_NULL || out_path == CLKS_NULL || out_size < 2U) {
        return CLKS_FALSE;
    }

    if (path[0] != '/') {
        return CLKS_FALSE;
    }

    out_path[out_pos++] = '/';

    while (path[in_pos] == '/') {
        in_pos++;
    }

    while (path[in_pos] != '\0') {
        usize comp_start = in_pos;
        usize comp_len;

        while (path[in_pos] != '\0' && path[in_pos] != '/') {
            in_pos++;
        }

        comp_len = in_pos - comp_start;

        if (comp_len == 0U) {
            while (path[in_pos] == '/') {
                in_pos++;
            }
            continue;
        }

        if (comp_len == 1U && path[comp_start] == '.') {
            while (path[in_pos] == '/') {
                in_pos++;
            }
            continue;
        }

        if (comp_len == 2U && path[comp_start] == '.' && path[comp_start + 1U] == '.') {
            return CLKS_FALSE;
        }

        if (out_pos > 1U) {
            if (out_pos + 1U >= out_size) {
                return CLKS_FALSE;
            }
            out_path[out_pos++] = '/';
        }

        if (out_pos + comp_len >= out_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_path + out_pos, path + comp_start, comp_len);
        out_pos += comp_len;

        while (path[in_pos] == '/') {
            in_pos++;
        }
    }

    out_path[out_pos] = '\0';
    return CLKS_TRUE;
}

static clks_bool clks_disk_normalize_relative_path(const char *path, char *out_path, usize out_size) {
    usize in_pos = 0U;
    usize out_pos = 0U;

    if (path == CLKS_NULL || out_path == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    if (path[0] == '/') {
        return CLKS_FALSE;
    }

    while (path[in_pos] == '/') {
        in_pos++;
    }

    while (path[in_pos] != '\0') {
        usize comp_start = in_pos;
        usize comp_len;

        while (path[in_pos] != '\0' && path[in_pos] != '/') {
            in_pos++;
        }

        comp_len = in_pos - comp_start;

        if (comp_len == 0U) {
            while (path[in_pos] == '/') {
                in_pos++;
            }
            continue;
        }

        if (comp_len == 1U && path[comp_start] == '.') {
            while (path[in_pos] == '/') {
                in_pos++;
            }
            continue;
        }

        if (comp_len == 2U && path[comp_start] == '.' && path[comp_start + 1U] == '.') {
            return CLKS_FALSE;
        }

        if (out_pos != 0U) {
            if (out_pos + 1U >= out_size) {
                return CLKS_FALSE;
            }
            out_path[out_pos++] = '/';
        }

        if (out_pos + comp_len >= out_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_path + out_pos, path + comp_start, comp_len);
        out_pos += comp_len;

        while (path[in_pos] == '/') {
            in_pos++;
        }
    }

    out_path[out_pos] = '\0';
    return CLKS_TRUE;
}

static clks_bool clks_disk_path_to_relative(const char *path, char *out_relative, usize out_size) {
    char normalized[CLKS_DISK_PATH_MAX];
    usize mount_len;

    if (out_relative == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    if (clks_disk_mounted == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_normalize_absolute_path(path, normalized, sizeof(normalized)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    mount_len = clks_strlen(clks_disk_mount_path_buf);

    if (clks_strcmp(normalized, clks_disk_mount_path_buf) == 0) {
        out_relative[0] = '\0';
        return CLKS_TRUE;
    }

    if (mount_len >= clks_strlen(normalized)) {
        return CLKS_FALSE;
    }

    if (clks_disk_text_prefix_equals(normalized, clks_disk_mount_path_buf, mount_len) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (normalized[mount_len] != '/') {
        return CLKS_FALSE;
    }

    return clks_disk_normalize_relative_path(normalized + mount_len + 1U, out_relative, out_size);
}

static const char *clks_disk_basename(const char *relative_path) {
    usize len;
    usize i;

    if (relative_path == CLKS_NULL) {
        return "";
    }

    len = clks_strlen(relative_path);

    if (len == 0U) {
        return "";
    }

    for (i = len; i != 0U; i--) {
        if (relative_path[i - 1U] == '/') {
            return &relative_path[i];
        }
    }

    return relative_path;
}

static clks_bool clks_disk_split_parent(const char *relative_path, char *parent_out, usize out_size) {
    usize len;
    usize i;

    if (relative_path == CLKS_NULL || parent_out == CLKS_NULL || out_size == 0U) {
        return CLKS_FALSE;
    }

    len = clks_strlen(relative_path);

    if (len == 0U) {
        parent_out[0] = '\0';
        return CLKS_TRUE;
    }

    for (i = len; i != 0U; i--) {
        if (relative_path[i - 1U] == '/') {
            usize parent_len = i - 1U;

            if (parent_len >= out_size) {
                return CLKS_FALSE;
            }

            clks_memcpy(parent_out, relative_path, parent_len);
            parent_out[parent_len] = '\0';
            return CLKS_TRUE;
        }
    }

    parent_out[0] = '\0';
    return CLKS_TRUE;
}

static clks_bool clks_disk_node_has_heap_data(u16 index) {
    return ((clks_disk_nodes[index].flags & CLKS_DISK_NODE_FLAG_HEAP_DATA) != 0U) ? CLKS_TRUE : CLKS_FALSE;
}

static void clks_disk_node_set_heap_data(u16 index, clks_bool value) {
    if (value == CLKS_TRUE) {
        clks_disk_nodes[index].flags |= CLKS_DISK_NODE_FLAG_HEAP_DATA;
    } else {
        clks_disk_nodes[index].flags &= (u16)(~CLKS_DISK_NODE_FLAG_HEAP_DATA);
    }
}

static void clks_disk_node_release_heap_data(u16 index) {
    if (clks_disk_nodes[index].type != (u8)CLKS_DISK_NODE_FILE) {
        return;
    }

    if (clks_disk_node_has_heap_data(index) == CLKS_TRUE && clks_disk_nodes[index].data != CLKS_NULL) {
        clks_kfree((void *)clks_disk_nodes[index].data);
    }

    clks_disk_node_set_heap_data(index, CLKS_FALSE);
}

static void clks_disk_nodes_reset(void) {
    u16 i;

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        if (clks_disk_nodes[i].used == CLKS_FALSE) {
            continue;
        }

        clks_disk_node_release_heap_data(i);
    }

    clks_memset(clks_disk_nodes, 0, sizeof(clks_disk_nodes));
    clks_disk_nodes_used = 0U;
}

static i32 clks_disk_find_node_by_relative(const char *relative_path) {
    u16 i;

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        if (clks_disk_nodes[i].used == CLKS_FALSE) {
            continue;
        }

        if (clks_strcmp(clks_disk_nodes[i].path, relative_path) == 0) {
            return (i32)i;
        }
    }

    return -1;
}

static i32 clks_disk_alloc_slot(void) {
    u16 i;

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        if (clks_disk_nodes[i].used == CLKS_FALSE) {
            return (i32)i;
        }
    }

    if (clks_disk_nodes_used >= CLKS_DISK_MAX_NODES) {
        return -1;
    }

    clks_disk_nodes_used++;
    return (i32)(clks_disk_nodes_used - 1U);
}

static i32 clks_disk_create_or_update_node(const char *relative_path, u8 type, u16 parent, const void *data, u64 size) {
    i32 existing;
    i32 slot;
    usize path_len;

    if (relative_path == CLKS_NULL) {
        return -1;
    }

    path_len = clks_strlen(relative_path);

    if (path_len >= CLKS_DISK_PATH_MAX) {
        return -1;
    }

    existing = clks_disk_find_node_by_relative(relative_path);

    if (existing >= 0) {
        struct clks_disk_node *node = &clks_disk_nodes[(u16)existing];

        if (node->type != type) {
            return -1;
        }

        node->parent = parent;

        if (type == (u8)CLKS_DISK_NODE_FILE) {
            node->data = data;
            node->size = size;
            node->flags = 0U;
        }

        node->used = CLKS_TRUE;
        return existing;
    }

    slot = clks_disk_alloc_slot();

    if (slot < 0) {
        return -1;
    }

    clks_disk_nodes[(u16)slot].used = CLKS_TRUE;
    clks_disk_nodes[(u16)slot].type = type;
    clks_disk_nodes[(u16)slot].parent = parent;
    clks_disk_nodes[(u16)slot].flags = 0U;
    clks_disk_nodes[(u16)slot].data = (type == (u8)CLKS_DISK_NODE_FILE) ? data : CLKS_NULL;
    clks_disk_nodes[(u16)slot].size = (type == (u8)CLKS_DISK_NODE_FILE) ? size : 0ULL;
    clks_memcpy(clks_disk_nodes[(u16)slot].path, relative_path, path_len + 1U);

    return slot;
}

static clks_bool clks_disk_ensure_root(void) {
    if (clks_disk_create_or_update_node("", (u8)CLKS_DISK_NODE_DIR, 0U, CLKS_NULL, 0ULL) != 0) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_ensure_dir_hierarchy(const char *relative_dir_path) {
    char prefix[CLKS_DISK_PATH_MAX];
    usize cursor = 0U;
    usize i = 0U;
    u16 current_parent = 0U;

    prefix[0] = '\0';

    if (relative_dir_path == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (relative_dir_path[0] == '\0') {
        return CLKS_TRUE;
    }

    while (relative_dir_path[i] != '\0') {
        usize comp_start = i;
        usize comp_len;
        i32 node_index;

        while (relative_dir_path[i] != '\0' && relative_dir_path[i] != '/') {
            i++;
        }

        comp_len = i - comp_start;

        if (comp_len == 0U) {
            return CLKS_FALSE;
        }

        if (cursor != 0U) {
            if (cursor + 1U >= sizeof(prefix)) {
                return CLKS_FALSE;
            }
            prefix[cursor++] = '/';
        }

        if (cursor + comp_len >= sizeof(prefix)) {
            return CLKS_FALSE;
        }

        clks_memcpy(prefix + cursor, relative_dir_path + comp_start, comp_len);
        cursor += comp_len;
        prefix[cursor] = '\0';

        node_index = clks_disk_find_node_by_relative(prefix);

        if (node_index < 0) {
            node_index =
                clks_disk_create_or_update_node(prefix, (u8)CLKS_DISK_NODE_DIR, current_parent, CLKS_NULL, 0ULL);

            if (node_index < 0) {
                return CLKS_FALSE;
            }
        } else if (clks_disk_nodes[(u16)node_index].type != (u8)CLKS_DISK_NODE_DIR) {
            return CLKS_FALSE;
        }

        current_parent = (u16)node_index;

        if (relative_dir_path[i] == '/') {
            i++;
        }
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_build_file_payload(const void *data, u64 size, const void **out_data,
                                              clks_bool *out_heap_owned) {
    void *payload;

    if (out_data == CLKS_NULL || out_heap_owned == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (size == 0ULL) {
        *out_data = (const void *)clks_disk_empty_file_data;
        *out_heap_owned = CLKS_FALSE;
        return CLKS_TRUE;
    }

    if (data == CLKS_NULL) {
        return CLKS_FALSE;
    }

    payload = clks_kmalloc((usize)size);

    if (payload == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memcpy(payload, data, (usize)size);
    *out_data = (const void *)payload;
    *out_heap_owned = CLKS_TRUE;
    return CLKS_TRUE;
}

static clks_bool clks_disk_meta_is_present(void) {
    const u8 *header;

    if (clks_disk_ready == CLKS_FALSE || clks_disk_bytes == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_disk_bytes_len < (CLKS_DISK_META_OFFSET + CLKS_DISK_META_HEADER_SIZE)) {
        return CLKS_FALSE;
    }

    header = clks_disk_bytes + (usize)CLKS_DISK_META_OFFSET;

    if (clks_disk_bytes_equal(header, (const u8 *)CLKS_DISK_META_MAGIC, 8U) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return (clks_disk_read_u32(header + 8U) == CLKS_DISK_META_VERSION) ? CLKS_TRUE : CLKS_FALSE;
}

static clks_bool clks_disk_meta_flush(void) {
    u8 *meta_base;
    u64 max_entries;
    u64 entry_count = 0ULL;
    u64 data_cursor = CLKS_DISK_DATA_OFFSET;
    u16 i;

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE || clks_disk_bytes == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_disk_bytes_len < CLKS_DISK_DATA_OFFSET) {
        return CLKS_FALSE;
    }

    meta_base = clks_disk_bytes + (usize)CLKS_DISK_META_OFFSET;
    clks_memset(meta_base, 0, (usize)CLKS_DISK_META_SIZE);

    max_entries = (CLKS_DISK_META_SIZE - (u64)CLKS_DISK_META_HEADER_SIZE) / (u64)CLKS_DISK_META_ENTRY_SIZE;

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        struct clks_disk_node *node = &clks_disk_nodes[i];
        u8 *entry_ptr;

        if (node->used == CLKS_FALSE || node->path[0] == '\0') {
            continue;
        }

        if (entry_count >= max_entries) {
            return CLKS_FALSE;
        }

        entry_ptr = meta_base + CLKS_DISK_META_HEADER_SIZE + (usize)(entry_count * (u64)CLKS_DISK_META_ENTRY_SIZE);
        entry_ptr[0] = node->type;
        clks_disk_write_u64(entry_ptr + 4U, node->size);

        if (node->type == (u8)CLKS_DISK_NODE_FILE && node->size > 0ULL) {
            if (node->data == CLKS_NULL) {
                return CLKS_FALSE;
            }

            if (data_cursor + node->size < data_cursor || data_cursor + node->size > clks_disk_bytes_len) {
                return CLKS_FALSE;
            }

            clks_disk_write_u64(entry_ptr + 12U, data_cursor);
            clks_memcpy(clks_disk_bytes + (usize)data_cursor, node->data, (usize)node->size);
            data_cursor += node->size;
        } else {
            clks_disk_write_u64(entry_ptr + 12U, 0ULL);
        }

        clks_disk_copy_text((char *)(entry_ptr + 20U), CLKS_DISK_PATH_MAX, node->path);
        entry_count++;
    }

    clks_memcpy(meta_base, (const void *)CLKS_DISK_META_MAGIC, 8U);
    clks_disk_write_u32(meta_base + 8U, CLKS_DISK_META_VERSION);
    clks_disk_write_u32(meta_base + 12U, (u32)entry_count);
    clks_disk_write_u64(meta_base + 16U, data_cursor - CLKS_DISK_DATA_OFFSET);

    if (clks_disk_sync_bytes_to_hw(data_cursor) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_meta_load(void) {
    const u8 *meta_base;
    u64 entry_count;
    u64 max_entries;
    u64 i;

    if (clks_disk_meta_is_present() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    meta_base = clks_disk_bytes + (usize)CLKS_DISK_META_OFFSET;
    entry_count = (u64)clks_disk_read_u32(meta_base + 12U);
    max_entries = (CLKS_DISK_META_SIZE - (u64)CLKS_DISK_META_HEADER_SIZE) / (u64)CLKS_DISK_META_ENTRY_SIZE;

    if (entry_count > max_entries) {
        return CLKS_FALSE;
    }

    clks_disk_nodes_reset();

    if (clks_disk_ensure_root() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    for (i = 0ULL; i < entry_count; i++) {
        const u8 *entry_ptr = meta_base + CLKS_DISK_META_HEADER_SIZE + (usize)(i * (u64)CLKS_DISK_META_ENTRY_SIZE);
        u8 type = entry_ptr[0];
        u64 size = clks_disk_read_u64(entry_ptr + 4U);
        u64 data_offset = clks_disk_read_u64(entry_ptr + 12U);
        char path[CLKS_DISK_PATH_MAX];
        char normalized_path[CLKS_DISK_PATH_MAX];
        char parent[CLKS_DISK_PATH_MAX];
        i32 parent_index;
        const void *payload_data = (const void *)clks_disk_empty_file_data;
        clks_bool payload_heap_owned = CLKS_FALSE;
        i32 node_index;

        clks_memset(path, 0, sizeof(path));
        clks_memcpy(path, entry_ptr + 20U, CLKS_DISK_PATH_MAX - 1U);

        if (clks_disk_normalize_relative_path(path, normalized_path, sizeof(normalized_path)) == CLKS_FALSE) {
            continue;
        }

        if (normalized_path[0] == '\0') {
            continue;
        }

        if (type == (u8)CLKS_DISK_NODE_DIR) {
            (void)clks_disk_ensure_dir_hierarchy(normalized_path);
            continue;
        }

        if (type != (u8)CLKS_DISK_NODE_FILE) {
            continue;
        }

        if (clks_disk_split_parent(normalized_path, parent, sizeof(parent)) == CLKS_FALSE) {
            continue;
        }

        if (clks_disk_ensure_dir_hierarchy(parent) == CLKS_FALSE) {
            continue;
        }

        parent_index = clks_disk_find_node_by_relative(parent);

        if (parent_index < 0 || clks_disk_nodes[(u16)parent_index].type != (u8)CLKS_DISK_NODE_DIR) {
            continue;
        }

        if (size == 0ULL) {
            payload_data = (const void *)clks_disk_empty_file_data;
            payload_heap_owned = CLKS_FALSE;
        } else {
            void *payload;

            if (data_offset < CLKS_DISK_DATA_OFFSET || data_offset + size < data_offset ||
                data_offset + size > clks_disk_bytes_len) {
                continue;
            }

            payload = clks_kmalloc((usize)size);

            if (payload == CLKS_NULL) {
                continue;
            }

            clks_memcpy(payload, clks_disk_bytes + (usize)data_offset, (usize)size);
            payload_data = payload;
            payload_heap_owned = CLKS_TRUE;
        }

        node_index = clks_disk_create_or_update_node(normalized_path, (u8)CLKS_DISK_NODE_FILE, (u16)parent_index,
                                                     payload_data, size);

        if (node_index < 0) {
            if (payload_heap_owned == CLKS_TRUE) {
                clks_kfree((void *)payload_data);
            }
            continue;
        }

        clks_disk_node_set_heap_data((u16)node_index, payload_heap_owned);
    }

    return CLKS_TRUE;
}

static clks_bool clks_disk_detect_fat32(void) {
    const u8 *boot;
    const u8 fat32_sig[CLKS_DISK_FAT32_TYPE_LEN] = {'F', 'A', 'T', '3', '2'};

    if (clks_disk_ready == CLKS_FALSE || clks_disk_bytes == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (clks_disk_bytes_len < CLKS_DISK_SECTOR_SIZE) {
        return CLKS_FALSE;
    }

    boot = clks_disk_bytes;

    if (boot[CLKS_DISK_FAT32_BOOT_SIG_OFFSET] != 0x55U || boot[CLKS_DISK_FAT32_BOOT_SIG_OFFSET + 1U] != 0xAAU) {
        return CLKS_FALSE;
    }

    if (clks_disk_read_u16(boot + 11U) != (u16)CLKS_DISK_SECTOR_SIZE) {
        return CLKS_FALSE;
    }

    return clks_disk_bytes_equal(boot + CLKS_DISK_FAT32_TYPE_OFFSET, fat32_sig, CLKS_DISK_FAT32_TYPE_LEN);
}

static void clks_disk_label_to_boot_field(char *out_label, const char *label) {
    usize i = 0U;

    for (i = 0U; i < 11U; i++) {
        out_label[i] = ' ';
    }

    if (label == CLKS_NULL || label[0] == '\0') {
        const char default_label[11] = {'C', 'L', 'E', 'O', 'N', 'O', 'S', 'D', 'I', 'S', 'K'};
        for (i = 0U; i < 11U; i++) {
            out_label[i] = default_label[i];
        }
        return;
    }

    i = 0U;
    while (label[i] != '\0' && i < 11U) {
        char ch = label[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - ('a' - 'A'));
        }
        out_label[i] = ch;
        i++;
    }
}

static clks_bool clks_disk_write_fat32_boot_sector(const char *label) {
    u8 *boot = clks_disk_bytes;
    u8 *fsinfo;
    u8 *backup_boot;
    u64 total_sectors = clks_disk_sector_total;
    u8 sectors_per_cluster = 1U;
    u16 reserved = 64U;
    u8 fats = 2U;
    u32 fat_sectors = 1U;
    u32 iter;
    char label_field[11];

    if (total_sectors < 8192ULL) {
        return CLKS_FALSE;
    }

    if (total_sectors > 262144ULL) {
        sectors_per_cluster = 8U;
    } else if (total_sectors > 131072ULL) {
        sectors_per_cluster = 4U;
    } else {
        sectors_per_cluster = 1U;
    }

    for (iter = 0U; iter < 8U; iter++) {
        u64 data_sectors;
        u64 cluster_count;
        u64 next_fat;

        if (total_sectors <= (u64)reserved + ((u64)fats * (u64)fat_sectors)) {
            return CLKS_FALSE;
        }

        data_sectors = total_sectors - (u64)reserved - ((u64)fats * (u64)fat_sectors);
        cluster_count = data_sectors / (u64)sectors_per_cluster;
        next_fat = ((cluster_count + 2ULL) * 4ULL + (CLKS_DISK_SECTOR_SIZE - 1ULL)) / CLKS_DISK_SECTOR_SIZE;

        if (next_fat > 0xFFFFFFFFULL) {
            return CLKS_FALSE;
        }

        if ((u32)next_fat == fat_sectors) {
            break;
        }

        fat_sectors = (u32)next_fat;
    }

    if (fat_sectors == 0U) {
        return CLKS_FALSE;
    }

    clks_memset(boot, 0, CLKS_DISK_SECTOR_SIZE);
    boot[0] = 0xEBU;
    boot[1] = 0x58U;
    boot[2] = 0x90U;
    clks_memcpy(boot + 3U, "MSDOS5.0", 8U);
    clks_disk_write_u16(boot + 11U, (u16)CLKS_DISK_SECTOR_SIZE);
    boot[13] = sectors_per_cluster;
    clks_disk_write_u16(boot + 14U, reserved);
    boot[16] = fats;
    clks_disk_write_u16(boot + 17U, 0U);
    clks_disk_write_u16(boot + 19U, 0U);
    boot[21] = 0xF8U;
    clks_disk_write_u16(boot + 22U, 0U);
    clks_disk_write_u16(boot + 24U, 63U);
    clks_disk_write_u16(boot + 26U, 255U);
    clks_disk_write_u32(boot + 28U, 0U);
    clks_disk_write_u32(boot + 32U, (u32)total_sectors);
    clks_disk_write_u32(boot + 36U, fat_sectors);
    clks_disk_write_u16(boot + 40U, 0U);
    clks_disk_write_u16(boot + 42U, 0U);
    clks_disk_write_u32(boot + 44U, 2U);
    clks_disk_write_u16(boot + 48U, 1U);
    clks_disk_write_u16(boot + 50U, 6U);
    boot[64] = 0x80U;
    boot[66] = 0x29U;
    clks_disk_write_u32(boot + 67U, 0x434C4B53U);
    clks_disk_label_to_boot_field(label_field, label);
    clks_memcpy(boot + 71U, label_field, 11U);
    clks_memcpy(boot + 82U, "FAT32   ", 8U);
    boot[CLKS_DISK_FAT32_BOOT_SIG_OFFSET] = 0x55U;
    boot[CLKS_DISK_FAT32_BOOT_SIG_OFFSET + 1U] = 0xAAU;

    fsinfo = clks_disk_bytes + (usize)CLKS_DISK_SECTOR_SIZE;
    clks_memset(fsinfo, 0, CLKS_DISK_SECTOR_SIZE);
    clks_disk_write_u32(fsinfo + 0U, 0x41615252U);
    clks_disk_write_u32(fsinfo + 484U, 0x61417272U);
    clks_disk_write_u32(fsinfo + 488U, 0xFFFFFFFFU);
    clks_disk_write_u32(fsinfo + 492U, 0xFFFFFFFFU);
    fsinfo[CLKS_DISK_FAT32_BOOT_SIG_OFFSET] = 0x55U;
    fsinfo[CLKS_DISK_FAT32_BOOT_SIG_OFFSET + 1U] = 0xAAU;

    backup_boot = clks_disk_bytes + (usize)(6ULL * CLKS_DISK_SECTOR_SIZE);
    clks_memcpy(backup_boot, boot, CLKS_DISK_SECTOR_SIZE);

    {
        u64 fat_base = (u64)reserved * CLKS_DISK_SECTOR_SIZE;
        u32 fat_index;

        for (fat_index = 0U; fat_index < fats; fat_index++) {
            u64 offset = fat_base + ((u64)fat_index * (u64)fat_sectors * CLKS_DISK_SECTOR_SIZE);
            u8 *fat_ptr;

            if (offset + 12ULL > clks_disk_bytes_len) {
                return CLKS_FALSE;
            }

            fat_ptr = clks_disk_bytes + (usize)offset;
            clks_disk_write_u32(fat_ptr + 0U, 0x0FFFFFF8U);
            clks_disk_write_u32(fat_ptr + 4U, 0xFFFFFFFFU);
            clks_disk_write_u32(fat_ptr + 8U, 0x0FFFFFFFU);
        }
    }

    return CLKS_TRUE;
}

void clks_disk_init(void) {
    u64 detected_sectors = 0ULL;
    u64 cache_bytes;
    u64 alloc_bytes;
    u64 lba;

    clks_disk_ready = CLKS_FALSE;
    clks_disk_formatted = CLKS_FALSE;
    clks_disk_mounted = CLKS_FALSE;
    clks_disk_hw_backed = CLKS_FALSE;
    clks_disk_bytes = CLKS_NULL;
    clks_disk_bytes_len = 0ULL;
    clks_disk_sector_total = 0ULL;
    clks_disk_mount_path_buf[0] = '\0';
    clks_disk_nodes_reset();

    if (clks_disk_ata_identify(&detected_sectors) == CLKS_FALSE || detected_sectors == 0ULL) {
        clks_log(CLKS_LOG_WARN, "DISK", "NO ATA DISK DETECTED (CHECK QEMU -DRIVE)");
        return;
    }

    cache_bytes = detected_sectors * CLKS_DISK_SECTOR_SIZE;
    if (cache_bytes / CLKS_DISK_SECTOR_SIZE != detected_sectors) {
        clks_log(CLKS_LOG_WARN, "DISK", "DISK SIZE OVERFLOW");
        return;
    }

    if (cache_bytes < CLKS_DISK_MIN_BYTES) {
        clks_log(CLKS_LOG_WARN, "DISK", "ATA DISK TOO SMALL");
        clks_log_hex(CLKS_LOG_WARN, "DISK", "BYTES", cache_bytes);
        return;
    }

    if (cache_bytes > CLKS_DISK_CACHE_MAX_BYTES) {
        cache_bytes = CLKS_DISK_CACHE_MAX_BYTES;
        clks_log(CLKS_LOG_WARN, "DISK", "DISK CACHE TRUNCATED");
    }

    alloc_bytes = cache_bytes;
    while (alloc_bytes >= CLKS_DISK_MIN_BYTES) {
        clks_disk_bytes = (u8 *)clks_kmalloc((usize)alloc_bytes);
        if (clks_disk_bytes != CLKS_NULL) {
            cache_bytes = alloc_bytes;
            break;
        }

        alloc_bytes /= 2ULL;
        alloc_bytes -= (alloc_bytes % CLKS_DISK_SECTOR_SIZE);
    }

    if (clks_disk_bytes == CLKS_NULL) {
        clks_log(CLKS_LOG_WARN, "DISK", "DISK BACKEND ALLOCATION FAILED");
        return;
    }

    detected_sectors = cache_bytes / CLKS_DISK_SECTOR_SIZE;

    for (lba = 0ULL; lba < detected_sectors; lba++) {
        u8 *sector_ptr = clks_disk_bytes + (usize)(lba * CLKS_DISK_SECTOR_SIZE);
        if (clks_disk_ata_read_sector_hw(lba, sector_ptr) == CLKS_FALSE) {
            clks_log(CLKS_LOG_WARN, "DISK", "ATA READ FAILED DURING CACHE LOAD");
            clks_log_hex(CLKS_LOG_WARN, "DISK", "LBA", lba);
            clks_kfree(clks_disk_bytes);
            clks_disk_bytes = CLKS_NULL;
            return;
        }
    }

    clks_disk_bytes_len = cache_bytes;
    clks_disk_sector_total = detected_sectors;
    clks_disk_hw_backed = CLKS_TRUE;
    clks_disk_ready = CLKS_TRUE;
    clks_disk_formatted = clks_disk_detect_fat32();

    if (clks_disk_ensure_root() == CLKS_FALSE) {
        clks_log(CLKS_LOG_ERROR, "DISK", "FAILED TO INIT ROOT NODE");
        clks_disk_ready = CLKS_FALSE;
        return;
    }

    if (clks_disk_formatted == CLKS_TRUE) {
        if (clks_disk_meta_load() == CLKS_FALSE) {
            clks_log(CLKS_LOG_WARN, "DISK", "FAT32 DETECTED, META MISSING; RESET TO EMPTY");
            clks_disk_nodes_reset();
            (void)clks_disk_ensure_root();
            (void)clks_disk_meta_flush();
        }
    }

    (void)clks_disk_mount("/temp/disk");
    clks_log(CLKS_LOG_INFO, "DISK", "DISK BACKEND ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "DISK", "BYTES", clks_disk_bytes_len);
    clks_log_hex(CLKS_LOG_INFO, "DISK", "SECTORS", clks_disk_sector_total);
    clks_log_hex(CLKS_LOG_INFO, "DISK", "FAT32", (clks_disk_formatted == CLKS_TRUE) ? 1ULL : 0ULL);
    clks_log_hex(CLKS_LOG_INFO, "DISK", "HW_BACKED", (clks_disk_hw_backed == CLKS_TRUE) ? 1ULL : 0ULL);
}

clks_bool clks_disk_present(void) {
    return clks_disk_ready;
}

u64 clks_disk_size_bytes(void) {
    return (clks_disk_ready == CLKS_TRUE) ? clks_disk_bytes_len : 0ULL;
}

u64 clks_disk_sector_count(void) {
    return (clks_disk_ready == CLKS_TRUE) ? clks_disk_sector_total : 0ULL;
}

clks_bool clks_disk_read_sector(u64 lba, void *out_sector) {
    if (clks_disk_ready == CLKS_FALSE || out_sector == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (lba >= clks_disk_sector_total) {
        return CLKS_FALSE;
    }

    if (clks_disk_hw_backed == CLKS_TRUE) {
        if (clks_disk_ata_read_sector_hw(lba, out_sector) == CLKS_FALSE) {
            return CLKS_FALSE;
        }
        clks_memcpy(clks_disk_bytes + (usize)(lba * CLKS_DISK_SECTOR_SIZE), out_sector, (usize)CLKS_DISK_SECTOR_SIZE);
        return CLKS_TRUE;
    }

    clks_memcpy(out_sector, clks_disk_bytes + (usize)(lba * CLKS_DISK_SECTOR_SIZE), (usize)CLKS_DISK_SECTOR_SIZE);
    return CLKS_TRUE;
}

clks_bool clks_disk_write_sector(u64 lba, const void *sector_data) {
    if (clks_disk_ready == CLKS_FALSE || sector_data == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (lba >= clks_disk_sector_total) {
        return CLKS_FALSE;
    }

    clks_memcpy(clks_disk_bytes + (usize)(lba * CLKS_DISK_SECTOR_SIZE), sector_data, (usize)CLKS_DISK_SECTOR_SIZE);

    if (clks_disk_hw_backed == CLKS_TRUE) {
        if (clks_disk_ata_write_sector_hw(lba, sector_data) == CLKS_FALSE) {
            return CLKS_FALSE;
        }
        if (clks_disk_ata_cache_flush_hw() == CLKS_FALSE) {
            return CLKS_FALSE;
        }
    }

    return CLKS_TRUE;
}

clks_bool clks_disk_is_formatted_fat32(void) {
    return (clks_disk_ready == CLKS_TRUE && clks_disk_formatted == CLKS_TRUE) ? CLKS_TRUE : CLKS_FALSE;
}

clks_bool clks_disk_format_fat32(const char *label) {
    if (clks_disk_ready == CLKS_FALSE || clks_disk_bytes == CLKS_NULL) {
        return CLKS_FALSE;
    }

    clks_memset(clks_disk_bytes, 0, (usize)clks_disk_bytes_len);

    if (clks_disk_sync_bytes_to_hw(clks_disk_bytes_len) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_write_fat32_boot_sector(label) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_disk_nodes_reset();

    if (clks_disk_ensure_root() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    clks_disk_formatted = CLKS_TRUE;

    if (clks_disk_meta_flush() == CLKS_FALSE) {
        clks_disk_formatted = CLKS_FALSE;
        return CLKS_FALSE;
    }

    clks_log(CLKS_LOG_INFO, "DISK", "FAT32 FORMAT COMPLETE");
    return CLKS_TRUE;
}

clks_bool clks_disk_mount(const char *mount_path) {
    char normalized[CLKS_DISK_PATH_MAX];

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_normalize_absolute_path(mount_path, normalized, sizeof(normalized)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_strcmp(normalized, "/") == 0) {
        return CLKS_FALSE;
    }

    clks_disk_copy_text(clks_disk_mount_path_buf, sizeof(clks_disk_mount_path_buf), normalized);
    clks_disk_mounted = CLKS_TRUE;
    return CLKS_TRUE;
}

clks_bool clks_disk_is_mounted(void) {
    return (clks_disk_ready == CLKS_TRUE && clks_disk_mounted == CLKS_TRUE) ? CLKS_TRUE : CLKS_FALSE;
}

const char *clks_disk_mount_path(void) {
    if (clks_disk_mounted == CLKS_FALSE) {
        return "";
    }

    return clks_disk_mount_path_buf;
}

clks_bool clks_disk_path_in_mount(const char *path) {
    char relative[CLKS_DISK_PATH_MAX];
    return (clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_TRUE) ? CLKS_TRUE : CLKS_FALSE;
}

clks_bool clks_disk_stat(const char *path, u64 *out_type, u64 *out_size) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 node_index;

    if (out_type == CLKS_NULL || out_size == CLKS_NULL ||
        clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    node_index = clks_disk_find_node_by_relative(relative);

    if (node_index < 0) {
        return CLKS_FALSE;
    }

    *out_type =
        (clks_disk_nodes[(u16)node_index].type == (u8)CLKS_DISK_NODE_DIR) ? CLKS_DISK_NODE_DIR : CLKS_DISK_NODE_FILE;
    *out_size = clks_disk_nodes[(u16)node_index].size;
    return CLKS_TRUE;
}

const void *clks_disk_read_all(const char *path, u64 *out_size) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 node_index;

    if (clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_NULL;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return CLKS_NULL;
    }

    node_index = clks_disk_find_node_by_relative(relative);

    if (node_index < 0 || clks_disk_nodes[(u16)node_index].type != (u8)CLKS_DISK_NODE_FILE) {
        return CLKS_NULL;
    }

    if (out_size != CLKS_NULL) {
        *out_size = clks_disk_nodes[(u16)node_index].size;
    }

    if (clks_disk_nodes[(u16)node_index].size == 0ULL) {
        return (const void *)clks_disk_empty_file_data;
    }

    return clks_disk_nodes[(u16)node_index].data;
}

u64 clks_disk_count_children(const char *dir_path) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 dir_index;
    u64 count = 0ULL;
    u16 i;

    if (clks_disk_path_to_relative(dir_path, relative, sizeof(relative)) == CLKS_FALSE) {
        return 0ULL;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return 0ULL;
    }

    dir_index = clks_disk_find_node_by_relative(relative);

    if (dir_index < 0 || clks_disk_nodes[(u16)dir_index].type != (u8)CLKS_DISK_NODE_DIR) {
        return 0ULL;
    }

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        if (clks_disk_nodes[i].used == CLKS_FALSE) {
            continue;
        }

        if ((u16)dir_index == i) {
            continue;
        }

        if (clks_disk_nodes[i].parent == (u16)dir_index) {
            count++;
        }
    }

    return count;
}

clks_bool clks_disk_get_child_name(const char *dir_path, u64 index, char *out_name, usize out_name_size) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 dir_index;
    u64 current = 0ULL;
    u16 i;

    if (out_name == CLKS_NULL || out_name_size == 0U) {
        return CLKS_FALSE;
    }

    if (clks_disk_path_to_relative(dir_path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    dir_index = clks_disk_find_node_by_relative(relative);

    if (dir_index < 0 || clks_disk_nodes[(u16)dir_index].type != (u8)CLKS_DISK_NODE_DIR) {
        return CLKS_FALSE;
    }

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        const char *base;
        usize base_len;

        if (clks_disk_nodes[i].used == CLKS_FALSE || clks_disk_nodes[i].parent != (u16)dir_index ||
            i == (u16)dir_index) {
            continue;
        }

        if (current != index) {
            current++;
            continue;
        }

        base = clks_disk_basename(clks_disk_nodes[i].path);
        base_len = clks_strlen(base);

        if (base_len + 1U > out_name_size) {
            return CLKS_FALSE;
        }

        clks_memcpy(out_name, base, base_len + 1U);
        return CLKS_TRUE;
    }

    return CLKS_FALSE;
}

clks_bool clks_disk_mkdir(const char *path) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 node_index;

    if (clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (relative[0] == '\0') {
        return CLKS_TRUE;
    }

    if (clks_disk_ensure_dir_hierarchy(relative) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    node_index = clks_disk_find_node_by_relative(relative);

    if (node_index < 0 || clks_disk_nodes[(u16)node_index].type != (u8)CLKS_DISK_NODE_DIR) {
        return CLKS_FALSE;
    }

    return (clks_disk_meta_flush() == CLKS_TRUE) ? CLKS_TRUE : CLKS_FALSE;
}

clks_bool clks_disk_write_all(const char *path, const void *data, u64 size) {
    char relative[CLKS_DISK_PATH_MAX];
    char parent[CLKS_DISK_PATH_MAX];
    const void *payload_data = CLKS_NULL;
    clks_bool payload_heap_owned = CLKS_FALSE;
    i32 parent_index;
    i32 node_index;

    if (clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE || relative[0] == '\0') {
        return CLKS_FALSE;
    }

    if (clks_disk_split_parent(relative, parent, sizeof(parent)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ensure_dir_hierarchy(parent) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    parent_index = clks_disk_find_node_by_relative(parent);

    if (parent_index < 0 || clks_disk_nodes[(u16)parent_index].type != (u8)CLKS_DISK_NODE_DIR) {
        return CLKS_FALSE;
    }

    node_index = clks_disk_find_node_by_relative(relative);

    if (node_index >= 0 && clks_disk_nodes[(u16)node_index].type != (u8)CLKS_DISK_NODE_FILE) {
        return CLKS_FALSE;
    }

    if (clks_disk_build_file_payload(data, size, &payload_data, &payload_heap_owned) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (node_index >= 0) {
        clks_disk_node_release_heap_data((u16)node_index);
    }

    node_index =
        clks_disk_create_or_update_node(relative, (u8)CLKS_DISK_NODE_FILE, (u16)parent_index, payload_data, size);

    if (node_index < 0) {
        if (payload_heap_owned == CLKS_TRUE) {
            clks_kfree((void *)payload_data);
        }
        return CLKS_FALSE;
    }

    clks_disk_node_set_heap_data((u16)node_index, payload_heap_owned);

    if (clks_disk_meta_flush() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

clks_bool clks_disk_append(const char *path, const void *data, u64 size) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 node_index;
    const void *old_data;
    u64 old_size;
    u64 new_size;
    void *merged;

    if (clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE || relative[0] == '\0') {
        return CLKS_FALSE;
    }

    if (size > 0ULL && data == CLKS_NULL) {
        return CLKS_FALSE;
    }

    node_index = clks_disk_find_node_by_relative(relative);

    if (node_index < 0) {
        return clks_disk_write_all(path, data, size);
    }

    if (clks_disk_nodes[(u16)node_index].type != (u8)CLKS_DISK_NODE_FILE) {
        return CLKS_FALSE;
    }

    old_data = clks_disk_nodes[(u16)node_index].data;
    old_size = clks_disk_nodes[(u16)node_index].size;

    if (old_size > (0xFFFFFFFFFFFFFFFFULL - size)) {
        return CLKS_FALSE;
    }

    new_size = old_size + size;

    if (new_size == 0ULL) {
        return clks_disk_write_all(path, clks_disk_empty_file_data, 0ULL);
    }

    merged = clks_kmalloc((usize)new_size);

    if (merged == CLKS_NULL) {
        return CLKS_FALSE;
    }

    if (old_size > 0ULL && old_data != CLKS_NULL) {
        clks_memcpy(merged, old_data, (usize)old_size);
    }

    if (size > 0ULL) {
        clks_memcpy((u8 *)merged + (usize)old_size, data, (usize)size);
    }

    if (clks_disk_write_all(path, merged, new_size) == CLKS_FALSE) {
        clks_kfree(merged);
        return CLKS_FALSE;
    }

    clks_kfree(merged);
    return CLKS_TRUE;
}

clks_bool clks_disk_remove(const char *path) {
    char relative[CLKS_DISK_PATH_MAX];
    i32 node_index;
    u16 i;

    if (clks_disk_path_to_relative(path, relative, sizeof(relative)) == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    if (relative[0] == '\0') {
        return CLKS_FALSE;
    }

    node_index = clks_disk_find_node_by_relative(relative);

    if (node_index < 0) {
        return CLKS_FALSE;
    }

    if (clks_disk_nodes[(u16)node_index].type == (u8)CLKS_DISK_NODE_DIR) {
        for (i = 0U; i < clks_disk_nodes_used; i++) {
            if (clks_disk_nodes[i].used == CLKS_FALSE) {
                continue;
            }

            if (clks_disk_nodes[i].parent == (u16)node_index) {
                return CLKS_FALSE;
            }
        }
    }

    clks_disk_node_release_heap_data((u16)node_index);
    clks_memset(&clks_disk_nodes[(u16)node_index], 0, sizeof(clks_disk_nodes[(u16)node_index]));

    if (clks_disk_meta_flush() == CLKS_FALSE) {
        return CLKS_FALSE;
    }

    return CLKS_TRUE;
}

u64 clks_disk_node_count(void) {
    u16 i;
    u64 used = 0ULL;

    if (clks_disk_ready == CLKS_FALSE || clks_disk_formatted == CLKS_FALSE) {
        return 0ULL;
    }

    for (i = 0U; i < clks_disk_nodes_used; i++) {
        if (clks_disk_nodes[i].used == CLKS_TRUE) {
            used++;
        }
    }

    return used;
}
