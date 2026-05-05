#include "cmd_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "user/cleonos_user.h"

#define INSTALL_MOUNT_PATH "/temp/disk"
#define INSTALL_SECTOR_SIZE 512ULL
#define INSTALL_STAGE2_LBA 1ULL
#define INSTALL_SKIP_PREFIX INSTALL_MOUNT_PATH
#define INSTALL_COPY_CHUNK_SIZE 8192U
#define INSTALL_WHOLE_FILE_LIMIT (128ULL * 1024ULL)
#define INSTALL_PROGRESS_BAR_WIDTH 28U
#define INSTALL_PROGRESS_STEP_PERCENT 5ULL
#define INSTALL_KERNEL_SOURCE "/system/install/clks_kernel.elf"
#define INSTALL_KERNEL_TARGET INSTALL_MOUNT_PATH "/kernel.elf"
#define INSTALL_LIMINE_CONF_SOURCE "/system/install/limine-harddisk.conf"
#define INSTALL_LIMINE_SYS_SOURCE "/system/install/limine-bios.sys"
#define INSTALL_LIMINE_HDD_SOURCE "/system/install/limine-bios-hdd.bin"
#define INSTALL_USER_DB_TARGET INSTALL_MOUNT_PATH "/system/users.db"
#define INSTALL_MANIFEST_SOURCE "/system/install_manifest.db"
#define INSTALL_MANIFEST_TARGET INSTALL_MOUNT_PATH "/system/install_manifest.db"
#define INSTALL_MANIFEST_NEW_TARGET INSTALL_MOUNT_PATH "/system/install_manifest.new"
#define INSTALL_MANIFEST_PREV_TARGET INSTALL_MOUNT_PATH "/system/install_manifest.prev"
#define INSTALL_UPDATE_STATE_TARGET INSTALL_MOUNT_PATH "/system/update_state.db"
#define INSTALL_DRYRUN_MANIFEST "/temp/install2disk_manifest.new"
#define INSTALL_HOME_TARGET INSTALL_MOUNT_PATH "/home"
#define INSTALL_ROOT_HOME_TARGET INSTALL_MOUNT_PATH "/home/root"
#define INSTALL_ARG_MAX 96U
#define INSTALL_REPAIR_NAME_MAX 96U
#define INSTALL_SHA256_HEX_LEN 64U
#define INSTALL_MANIFEST_MAX_BYTES 32768ULL
#define INSTALL_UPDATE_MAX_ENTRIES 192U
#define INSTALL_UPDATE_VERIFY_SAMPLE_MAX 5U

typedef unsigned char install_u8;

#define INSTALL_PTR_LOW_GUARD 0x10000ULL

typedef struct install_sha256_ctx {
    install_u8 data[64];
    unsigned int datalen;
    u64 bitlen;
    unsigned int state[8];
} install_sha256_ctx;

typedef struct install_progress {
    const char *label;
    u64 total_items;
    u64 total_bytes;
    u64 done_items;
    u64 done_bytes;
    u64 last_percent;
    int started;
} install_progress;

typedef struct install_manifest_writer {
    u64 files;
    u64 file_bytes;
    u64 manifest_bytes;
} install_manifest_writer;

typedef int (*install_manifest_include_fn)(const char *logical_path);

typedef struct install_verify_result {
    const char *root_path;
    u64 required_checked;
    u64 manifest_entries;
    u64 files_checked;
    u64 bytes_checked;
    u64 missing;
    u64 mismatches;
    u64 corrupt;
    u64 boot_errors;
} install_verify_result;

typedef struct install_manifest_entry {
    char path[USH_PATH_MAX];
    u64 size;
    char hash[INSTALL_SHA256_HEX_LEN + 1U];
} install_manifest_entry;

typedef struct install_update_result {
    u64 added;
    u64 updated;
    u64 unchanged;
    u64 deleted;
    u64 copied_files;
    u64 copied_bytes;
    u64 verify_failed;
    char verify_failed_paths[INSTALL_UPDATE_VERIFY_SAMPLE_MAX][USH_PATH_MAX];
} install_update_result;

static install_manifest_entry install_update_old_entries[INSTALL_UPDATE_MAX_ENTRIES];
static install_manifest_entry install_update_new_entries[INSTALL_UPDATE_MAX_ENTRIES];
static int install_update_last_failure_was_verify;

#define INSTALL_TEXT(en, zh) ((ush_locale_is_zh() != 0) ? (zh) : (en))

static const char *install_stage_label(const char *name) {
    if (name == (const char *)0) {
        return INSTALL_TEXT("stage", "阶段 (stage)");
    }
    if (strcmp(name, "verify hard disk install") == 0) {
        return INSTALL_TEXT("verify hard disk install", "验证硬盘安装 (verify hard disk install)");
    }
    if (strcmp(name, "setup root account") == 0) {
        return INSTALL_TEXT("setup root account", "设置 root 账户 (setup root account)");
    }
    if (strcmp(name, "repair component") == 0) {
        return INSTALL_TEXT("repair component", "修复组件 (repair component)");
    }
    if (strcmp(name, "plan shell update") == 0) {
        return INSTALL_TEXT("plan shell update", "规划外壳更新 (plan shell update)");
    }
    if (strcmp(name, "update shell") == 0) {
        return INSTALL_TEXT("update shell", "更新外壳 (update shell)");
    }
    if (strcmp(name, "refresh install manifest") == 0) {
        return INSTALL_TEXT("refresh install manifest", "刷新安装清单 (refresh install manifest)");
    }
    if (strcmp(name, "update kernel and shell") == 0) {
        return INSTALL_TEXT("update kernel and shell", "更新内核和外壳 (update kernel and shell)");
    }
    if (strcmp(name, "update kernel") == 0) {
        return INSTALL_TEXT("update kernel", "更新内核 (update kernel)");
    }
    if (strcmp(name, "scan root filesystem") == 0) {
        return INSTALL_TEXT("scan root filesystem", "扫描根文件系统 (scan root filesystem)");
    }
    if (strcmp(name, "format FAT32") == 0) {
        return INSTALL_TEXT("format FAT32", "格式化 FAT32 (format FAT32)");
    }
    if (strcmp(name, "mount target disk") == 0) {
        return INSTALL_TEXT("mount target disk", "挂载目标硬盘 (mount target disk)");
    }
    if (strcmp(name, "copy root filesystem") == 0) {
        return INSTALL_TEXT("copy root filesystem", "复制根文件系统 (copy root filesystem)");
    }
    if (strcmp(name, "install kernel and Limine files") == 0) {
        return INSTALL_TEXT("install kernel and Limine files", "安装内核和 Limine 文件 (install kernel and Limine files)");
    }
    if (strcmp(name, "write Limine BIOS boot stages") == 0) {
        return INSTALL_TEXT("write Limine BIOS boot stages", "写入 Limine BIOS 启动阶段 (write Limine BIOS boot stages)");
    }
    if (strcmp(name, "generate install manifest") == 0) {
        return INSTALL_TEXT("generate install manifest", "生成安装清单 (generate install manifest)");
    }
    return name;
}

static const char *install_progress_label(const char *label) {
    if (label == (const char *)0) {
        return INSTALL_TEXT("progress", "进度 (progress)");
    }
    if (strcmp(label, "repair file") == 0) {
        return INSTALL_TEXT("repair file", "修复文件 (repair file)");
    }
    if (strcmp(label, "repair limine config") == 0) {
        return INSTALL_TEXT("repair limine config", "修复 Limine 配置 (repair limine config)");
    }
    if (strcmp(label, "repair limine sys") == 0) {
        return INSTALL_TEXT("repair limine sys", "修复 Limine sys 文件 (repair limine sys)");
    }
    if (strcmp(label, "repair bootloader") == 0) {
        return INSTALL_TEXT("repair bootloader", "修复引导器 (repair bootloader)");
    }
    if (strcmp(label, "plan shell files") == 0) {
        return INSTALL_TEXT("plan shell files", "规划外壳文件 (plan shell files)");
    }
    if (strcmp(label, "update shell files") == 0) {
        return INSTALL_TEXT("update shell files", "更新外壳文件 (update shell files)");
    }
    if (strcmp(label, "update kernel") == 0) {
        return INSTALL_TEXT("update kernel", "更新内核 (update kernel)");
    }
    if (strcmp(label, "copy root filesystem") == 0) {
        return INSTALL_TEXT("copy root filesystem", "复制根文件系统 (copy root filesystem)");
    }
    return label;
}

static void install_puts_i18n(const char *en, const char *zh) {
    (void)puts(INSTALL_TEXT(en, zh));
}

static void install_stage(const char *name) {
    (void)printf("install2disk: == %s ==\n", install_stage_label(name));
}

static u64 install_progress_percent(const install_progress *progress) {
    u64 percent;

    if (progress == (const install_progress *)0) {
        return 0ULL;
    }

    if (progress->total_bytes > 0ULL) {
        percent = (progress->done_bytes * 100ULL) / progress->total_bytes;
    } else if (progress->total_items > 0ULL) {
        percent = (progress->done_items * 100ULL) / progress->total_items;
    } else {
        percent = 100ULL;
    }

    return (percent > 100ULL) ? 100ULL : percent;
}

static void install_progress_print(install_progress *progress, int force) {
    char bar[INSTALL_PROGRESS_BAR_WIDTH + 1U];
    u64 percent;
    unsigned int filled;
    unsigned int i;

    if (progress == (install_progress *)0) {
        return;
    }

    percent = install_progress_percent(progress);
    if (force == 0 && progress->started != 0 && percent < 100ULL &&
        percent < progress->last_percent + INSTALL_PROGRESS_STEP_PERCENT) {
        return;
    }

    progress->started = 1;
    progress->last_percent = percent;
    filled = (unsigned int)((percent * (u64)INSTALL_PROGRESS_BAR_WIDTH) / 100ULL);
    if (filled > INSTALL_PROGRESS_BAR_WIDTH) {
        filled = INSTALL_PROGRESS_BAR_WIDTH;
    }

    for (i = 0U; i < INSTALL_PROGRESS_BAR_WIDTH; i++) {
        bar[i] = (i < filled) ? '#' : '-';
    }
    bar[INSTALL_PROGRESS_BAR_WIDTH] = '\0';

    (void)printf(INSTALL_TEXT("install2disk: %s [%s] %3llu%% %llu/%llu items %llu/%llu bytes\n",
                              "install2disk: %s [%s] %3llu%% %llu/%llu 项 (items) %llu/%llu 字节 (bytes)\n"),
                 install_progress_label(progress->label), bar, (unsigned long long)percent,
                 (unsigned long long)progress->done_items, (unsigned long long)progress->total_items,
                 (unsigned long long)progress->done_bytes, (unsigned long long)progress->total_bytes);
}

static void install_progress_add_done_bytes(install_progress *progress, u64 bytes) {
    if (progress == (install_progress *)0) {
        return;
    }

    progress->done_bytes += bytes;
    if (progress->done_bytes > progress->total_bytes && progress->total_bytes > 0ULL) {
        progress->done_bytes = progress->total_bytes;
    }
    install_progress_print(progress, 0);
}

static void install_progress_finish_item(install_progress *progress) {
    if (progress == (install_progress *)0) {
        return;
    }

    progress->done_items += 1ULL;
    if (progress->done_items > progress->total_items && progress->total_items > 0ULL) {
        progress->done_items = progress->total_items;
    }
    install_progress_print(progress, 0);
}

static void install_progress_plan_file(install_progress *progress, const char *path, u64 copies) {
    u64 size;

    if (progress == (install_progress *)0 || path == (const char *)0 || copies == 0ULL) {
        return;
    }

    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1) {
        return;
    }

    progress->total_items += copies;
    progress->total_bytes += size * copies;
}

static unsigned int install_sha256_rotr(unsigned int value, unsigned int count) {
    return (value >> count) | (value << (32U - count));
}

static unsigned int install_sha256_load_be32(const install_u8 *data) {
    if (data == (const install_u8 *)0 || (u64)(usize)data < INSTALL_PTR_LOW_GUARD) {
        return 0U;
    }

    return ((unsigned int)data[0] << 24U) | ((unsigned int)data[1] << 16U) | ((unsigned int)data[2] << 8U) |
           (unsigned int)data[3];
}

static void install_sha256_store_be32(unsigned int value, install_u8 *out) {
    if (out == (install_u8 *)0 || (u64)(usize)out < INSTALL_PTR_LOW_GUARD) {
        return;
    }

    out[0] = (install_u8)((value >> 24U) & 0xFFU);
    out[1] = (install_u8)((value >> 16U) & 0xFFU);
    out[2] = (install_u8)((value >> 8U) & 0xFFU);
    out[3] = (install_u8)(value & 0xFFU);
}

static void install_sha256_transform(install_sha256_ctx *ctx, const install_u8 data[64]) {
    static const unsigned int k[64] = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U};
    unsigned int m[64];
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    unsigned int e;
    unsigned int f;
    unsigned int g;
    unsigned int h;
    unsigned int i;

    if (ctx == (install_sha256_ctx *)0 || data == (const install_u8 *)0 ||
        (u64)(usize)ctx < INSTALL_PTR_LOW_GUARD || (u64)(usize)data < INSTALL_PTR_LOW_GUARD) {
        return;
    }

    for (i = 0U; i < 16U; i++) {
        m[i] = install_sha256_load_be32(data + (i * 4U));
    }
    for (i = 16U; i < 64U; i++) {
        unsigned int s0 =
            install_sha256_rotr(m[i - 15U], 7U) ^ install_sha256_rotr(m[i - 15U], 18U) ^ (m[i - 15U] >> 3U);
        unsigned int s1 =
            install_sha256_rotr(m[i - 2U], 17U) ^ install_sha256_rotr(m[i - 2U], 19U) ^ (m[i - 2U] >> 10U);
        m[i] = m[i - 16U] + s0 + m[i - 7U] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0U; i < 64U; i++) {
        unsigned int s1 = install_sha256_rotr(e, 6U) ^ install_sha256_rotr(e, 11U) ^ install_sha256_rotr(e, 25U);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int temp1 = h + s1 + ch + k[i] + m[i];
        unsigned int s0 = install_sha256_rotr(a, 2U) ^ install_sha256_rotr(a, 13U) ^ install_sha256_rotr(a, 22U);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void install_sha256_init(install_sha256_ctx *ctx) {
    if (ctx == (install_sha256_ctx *)0 || (u64)(usize)ctx < INSTALL_PTR_LOW_GUARD) {
        return;
    }

    ctx->datalen = 0U;
    ctx->bitlen = 0ULL;
    ctx->state[0] = 0x6A09E667U;
    ctx->state[1] = 0xBB67AE85U;
    ctx->state[2] = 0x3C6EF372U;
    ctx->state[3] = 0xA54FF53AU;
    ctx->state[4] = 0x510E527FU;
    ctx->state[5] = 0x9B05688CU;
    ctx->state[6] = 0x1F83D9ABU;
    ctx->state[7] = 0x5BE0CD19U;
}

static void install_sha256_update(install_sha256_ctx *ctx, const install_u8 *data, u64 len) {
    u64 i;

    if (ctx == (install_sha256_ctx *)0 || data == (const install_u8 *)0 ||
        (u64)(usize)ctx < INSTALL_PTR_LOW_GUARD || (u64)(usize)data < INSTALL_PTR_LOW_GUARD) {
        return;
    }

    for (i = 0ULL; i < len; i++) {
        if (ctx->datalen >= 64U) {
            install_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512ULL;
            ctx->datalen = 0U;
        }
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64U) {
            install_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512ULL;
            ctx->datalen = 0U;
        }
    }
}

static void install_sha256_final(install_sha256_ctx *ctx, install_u8 hash[32]) {
    unsigned int i;
    u64 bitlen;

    if (ctx == (install_sha256_ctx *)0 || hash == (install_u8 *)0 ||
        (u64)(usize)ctx < INSTALL_PTR_LOW_GUARD || (u64)(usize)hash < INSTALL_PTR_LOW_GUARD) {
        return;
    }

    if (ctx->datalen > 64U) {
        ctx->datalen = 0U;
    }
    i = ctx->datalen;

    if (ctx->datalen < 56U) {
        ctx->data[i++] = 0x80U;
        while (i < 56U) {
            ctx->data[i++] = 0U;
        }
    } else {
        ctx->data[i++] = 0x80U;
        while (i < 64U) {
            ctx->data[i++] = 0U;
        }
        install_sha256_transform(ctx, ctx->data);
        for (i = 0U; i < 56U; i++) {
            ctx->data[i] = 0U;
        }
    }

    bitlen = ctx->bitlen + ((u64)ctx->datalen * 8ULL);
    ctx->data[56] = (install_u8)((bitlen >> 56U) & 0xFFU);
    ctx->data[57] = (install_u8)((bitlen >> 48U) & 0xFFU);
    ctx->data[58] = (install_u8)((bitlen >> 40U) & 0xFFU);
    ctx->data[59] = (install_u8)((bitlen >> 32U) & 0xFFU);
    ctx->data[60] = (install_u8)((bitlen >> 24U) & 0xFFU);
    ctx->data[61] = (install_u8)((bitlen >> 16U) & 0xFFU);
    ctx->data[62] = (install_u8)((bitlen >> 8U) & 0xFFU);
    ctx->data[63] = (install_u8)(bitlen & 0xFFU);
    install_sha256_transform(ctx, ctx->data);

    for (i = 0U; i < 8U; i++) {
        install_sha256_store_be32(ctx->state[i], hash + (i * 4U));
    }
}

static char install_hex_digit(u64 value) {
    value &= 0xFULL;
    return (value < 10ULL) ? (char)('0' + value) : (char)('a' + (value - 10ULL));
}

static int install_sha256_file_hex(const char *path, char out_hex[INSTALL_SHA256_HEX_LEN + 1U]) {
    static install_u8 buf[INSTALL_COPY_CHUNK_SIZE];
    install_sha256_ctx ctx;
    install_u8 hash[32];
    u64 fd;
    u64 i;

    if (path == (const char *)0 || out_hex == (char *)0) {
        return 0;
    }
    out_hex[0] = '\0';

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        return 0;
    }

    install_sha256_init(&ctx);
    for (;;) {
        u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            return 0;
        }
        if (got == 0ULL) {
            break;
        }
        install_sha256_update(&ctx, buf, got);
    }
    (void)cleonos_sys_fd_close(fd);

    install_sha256_final(&ctx, hash);
    for (i = 0ULL; i < 32ULL; i++) {
        out_hex[i * 2ULL] = install_hex_digit(((u64)hash[i]) >> 4U);
        out_hex[i * 2ULL + 1ULL] = install_hex_digit((u64)hash[i]);
    }
    out_hex[INSTALL_SHA256_HEX_LEN] = '\0';
    return 1;
}

static int install_path_join(char *out, u64 out_size, const char *base, const char *name) {
    u64 base_len;
    u64 name_len;
    u64 need;

    if (out == (char *)0 || out_size == 0ULL || base == (const char *)0 || name == (const char *)0) {
        return 0;
    }

    base_len = (u64)strlen(base);
    name_len = (u64)strlen(name);
    need = base_len + ((base_len > 1ULL) ? 1ULL : 0ULL) + name_len + 1ULL;
    if (need > out_size) {
        return 0;
    }

    (void)snprintf(out, (unsigned long)out_size, "%s%s%s", base, (base_len > 1ULL) ? "/" : "", name);
    return 1;
}

static int install_path_is_under(const char *path, const char *prefix) {
    u64 prefix_len;

    if (path == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    prefix_len = (u64)strlen(prefix);
    if (strncmp(path, prefix, (unsigned long)prefix_len) != 0) {
        return 0;
    }

    return (path[prefix_len] == '\0' || path[prefix_len] == '/') ? 1 : 0;
}

static int install_mkdir(const char *path) {
    u64 type;

    type = cleonos_sys_fs_stat_type(path);
    if (type == 2ULL) {
        return 1;
    }

    if (cleonos_sys_fs_mkdir(path) == 0ULL) {
        (void)printf(INSTALL_TEXT("install2disk: mkdir failed: %s\n",
                                  "install2disk: 创建目录失败 (mkdir failed): %s\n"),
                     path);
        return 0;
    }

    return 1;
}

static int install_mkdir_parents_for_file(const char *file_path) {
    char path[USH_PATH_MAX];
    u64 i;

    if (file_path == (const char *)0 || file_path[0] != '/') {
        return 0;
    }

    if (strlen(file_path) >= sizeof(path)) {
        return 0;
    }

    (void)snprintf(path, (unsigned long)sizeof(path), "%s", file_path);
    for (i = 1ULL; path[i] != '\0'; i++) {
        if (path[i] != '/') {
            continue;
        }

        path[i] = '\0';
        if (path[0] != '\0' && install_mkdir(path) == 0) {
            return 0;
        }
        path[i] = '/';
    }

    return 1;
}

static int install_prepare_limine_dirs(void) {
    if (install_mkdir(INSTALL_MOUNT_PATH "/boot") == 0 || install_mkdir(INSTALL_MOUNT_PATH "/boot/limine") == 0 ||
        install_mkdir(INSTALL_MOUNT_PATH "/limine") == 0) {
        return 0;
    }

    return 1;
}

static int install_mount_existing_disk(void);
static int install_update_kernel(void);

static void install_print_usage(void) {
    install_puts_i18n("usage:", "用法 (usage):");
    (void)puts("  install2disk");
    (void)puts("  install2disk update");
    (void)puts("  install2disk update-shell");
    (void)puts("  install2disk update shell");
    (void)puts("  install2disk update-shell --dry-run");
    (void)puts("  install2disk update-kernel");
    (void)puts("  install2disk update kernel");
    (void)puts("  install2disk verify");
    (void)puts("  install2disk repair");
    (void)puts("  install2disk repair manifest");
    (void)puts("  install2disk repair limine");
    (void)puts("  install2disk repair bootloader");
    (void)puts("  install2disk repair limine-conf");
    (void)puts("  install2disk repair limine-sys");
    (void)puts("  install2disk repair shell <app|app.elf>");
    (void)puts("  install2disk repair uwm <app|app.elf>");
    (void)puts("  install2disk repair driver <app|app.elf>");
    (void)puts("  install2disk repair system <app|app.elf>");
    (void)puts("  install2disk repair path <absolute-path-on-target>");
    if (ush_locale_is_zh() != 0) {
        (void)puts("说明: update 更新内核+外壳，verify 验证安装，repair 修复单独组件。");
    }
}

static void install_read_line(const char *prompt, char *out, u64 out_size) {
    u64 len = 0ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (prompt != (const char *)0) {
        (void)printf("%s", prompt);
    }

    for (;;) {
        int ch = getchar();

        if (ch == EOF || ch == '\n') {
            putchar('\n');
            out[len] = '\0';
            return;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\b' || ch == 0x7F) {
            if (len > 0ULL) {
                len--;
                out[len] = '\0';
                (void)printf("\b \b");
            }
            continue;
        }

        if (len + 1ULL < out_size) {
            out[len++] = (char)ch;
            putchar(ch);
        }
    }
}

static int install_name_has_elf_suffix(const char *name) {
    u64 len;

    if (name == (const char *)0) {
        return 0;
    }

    len = (u64)strlen(name);
    if (len < 4ULL) {
        return 0;
    }

    return (strcmp(name + len - 4ULL, ".elf") == 0) ? 1 : 0;
}

static int install_component_name_valid(const char *name) {
    u64 i;

    if (name == (const char *)0 || name[0] == '\0') {
        return 0;
    }

    if (strlen(name) >= INSTALL_REPAIR_NAME_MAX) {
        return 0;
    }

    for (i = 0ULL; name[i] != '\0'; i++) {
        char ch = name[i];

        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
            ch == '-' || ch == '.') {
            continue;
        }

        return 0;
    }

    return 1;
}

static int install_build_component_path(char *out, u64 out_size, const char *dir, const char *name) {
    char normalized[INSTALL_REPAIR_NAME_MAX + 5U];

    if (out == (char *)0 || out_size == 0ULL || dir == (const char *)0 || name == (const char *)0) {
        return 0;
    }

    if (install_component_name_valid(name) == 0) {
        return 0;
    }

    if (install_name_has_elf_suffix(name) != 0) {
        (void)snprintf(normalized, (unsigned long)sizeof(normalized), "%s", name);
    } else {
        (void)snprintf(normalized, (unsigned long)sizeof(normalized), "%s.elf", name);
    }

    return install_path_join(out, out_size, dir, normalized);
}

static int install_build_target_path(char *out, u64 out_size, const char *src_path) {
    if (out == (char *)0 || out_size == 0ULL || src_path == (const char *)0 || src_path[0] != '/') {
        return 0;
    }

    if (install_path_is_under(src_path, INSTALL_MOUNT_PATH) != 0 || strcmp(src_path, "/proc") == 0 ||
        install_path_is_under(src_path, "/proc") != 0 || strcmp(src_path, "/dev") == 0 ||
        install_path_is_under(src_path, "/dev") != 0) {
        return 0;
    }

    if ((u64)snprintf(out, (unsigned long)out_size, "%s%s", INSTALL_MOUNT_PATH, src_path) >= out_size) {
        return 0;
    }

    return 1;
}

static int install_prompt_choice(const char *prompt, const char *valid_choices, int default_choice) {
    int ch;

    if (prompt != (const char *)0) {
        (void)printf("%s", prompt);
    }

    for (;;) {
        ch = getchar();
        if (ch == EOF) {
            return default_choice;
        }

        if (ch == '\r' || ch == '\n') {
            putchar('\n');
            return default_choice;
        }

        if (ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a';
        }

        if (valid_choices != (const char *)0 && strchr(valid_choices, ch) != (char *)0) {
            putchar(ch);
            putchar('\n');
            return ch;
        }
    }
}

static int install_copy_file(const char *src, const char *dst, u64 *copied_files, u64 *copied_bytes,
                             install_progress *progress) {
    static char buffer[INSTALL_COPY_CHUNK_SIZE];
    u64 size;
    u64 dst_type;
    u64 got;
    u64 wrote;
    u64 src_fd;
    u64 file_offset = 0ULL;

    size = cleonos_sys_fs_stat_size(src);
    if (size == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: stat failed: %s\n",
                                  "install2disk: 获取文件信息失败 (stat failed): %s\n"),
                     src);
        return 0;
    }

    dst_type = cleonos_sys_fs_stat_type(dst);
    if (dst_type == 2ULL) {
        (void)printf(INSTALL_TEXT("install2disk: target is directory: %s\n",
                                  "install2disk: 目标是目录 (target is directory): %s\n"),
                     dst);
        return 0;
    }
    if (dst_type == 1ULL && cleonos_sys_fs_remove(dst) == 0ULL) {
        (void)printf(INSTALL_TEXT("install2disk: remove old target failed: %s\n",
                                  "install2disk: 删除旧目标失败 (remove old target failed): %s\n"),
                     dst);
        return 0;
    }

    if (size == 0ULL) {
        if (cleonos_sys_fs_write(dst, "", 0ULL) == 0ULL) {
            (void)printf(INSTALL_TEXT("install2disk: create failed: %s\n",
                                      "install2disk: 创建失败 (create failed): %s\n"),
                         dst);
            return 0;
        }

        if (cleonos_sys_fs_stat_size(dst) != 0ULL) {
            (void)printf(INSTALL_TEXT("install2disk: verify failed: %s\n",
                                      "install2disk: 验证失败 (verify failed): %s\n"),
                         dst);
            return 0;
        }

        if (copied_files != (u64 *)0) {
            *copied_files += 1ULL;
        }
        install_progress_finish_item(progress);
        return 1;
    }

    if (size <= INSTALL_WHOLE_FILE_LIMIT) {
        char *whole = (char *)malloc((size_t)size);

        if (whole != (char *)0) {
            src_fd = cleonos_sys_fd_open(src, CLEONOS_O_RDONLY, 0ULL);
            if (src_fd == (u64)-1) {
                free(whole);
                (void)printf(INSTALL_TEXT("install2disk: open read failed: %s\n",
                                          "install2disk: 打开读取失败 (open read failed): %s\n"),
                             src);
                return 0;
            }

            while (file_offset < size) {
                u64 want = size - file_offset;

                if (want > (u64)INSTALL_COPY_CHUNK_SIZE) {
                    want = (u64)INSTALL_COPY_CHUNK_SIZE;
                }

                got = cleonos_sys_fd_read(src_fd, whole + (size_t)file_offset, want);
                if (got == (u64)-1 || got == 0ULL) {
                    (void)cleonos_sys_fd_close(src_fd);
                    free(whole);
                    (void)printf(INSTALL_TEXT("install2disk: read failed: %s\n",
                                              "install2disk: 读取失败 (read failed): %s\n"),
                                 src);
                    (void)printf(INSTALL_TEXT("install2disk: src=%s mode=whole offset=%llu want=%llu got=%llu size=%llu\n",
                                              "install2disk: 源 (src)=%s 模式 (mode)=whole 偏移 (offset)=%llu 期望 (want)=%llu 得到 (got)=%llu 大小 (size)=%llu\n"),
                                 src,
                                 (unsigned long long)file_offset, (unsigned long long)want,
                                 (unsigned long long)got, (unsigned long long)size);
                    return 0;
                }

                file_offset += got;
            }

            (void)cleonos_sys_fd_close(src_fd);
            wrote = cleonos_sys_fs_write(dst, whole, size);
            free(whole);

            if (wrote != size) {
                (void)printf(INSTALL_TEXT("install2disk: write failed: %s\n",
                                          "install2disk: 写入失败 (write failed): %s\n"),
                             dst);
                (void)printf(INSTALL_TEXT("install2disk: src=%s mode=whole size=%llu wrote=%llu\n",
                                          "install2disk: 源 (src)=%s 模式 (mode)=whole 大小 (size)=%llu 写入 (wrote)=%llu\n"),
                             src,
                             (unsigned long long)size, (unsigned long long)wrote);
                return 0;
            }

            if (copied_bytes != (u64 *)0) {
                *copied_bytes += size;
            }
            if (copied_files != (u64 *)0) {
                *copied_files += 1ULL;
            }
            install_progress_add_done_bytes(progress, size);
            install_progress_finish_item(progress);
            if (cleonos_sys_fs_stat_size(dst) != size) {
                (void)printf(INSTALL_TEXT("install2disk: verify failed: %s\n",
                                          "install2disk: 验证失败 (verify failed): %s\n"),
                             dst);
                (void)printf(INSTALL_TEXT("install2disk: expected=%llu actual=%llu\n",
                                          "install2disk: 期望 (expected)=%llu 实际 (actual)=%llu\n"),
                             (unsigned long long)size,
                             (unsigned long long)cleonos_sys_fs_stat_size(dst));
                return 0;
            }
            return 1;
        }

        (void)printf(INSTALL_TEXT("install2disk: whole-file buffer unavailable, falling back: %s\n",
                                  "install2disk: 整文件缓冲不可用，回退流式复制 (falling back): %s\n"),
                     src);
    }

    src_fd = cleonos_sys_fd_open(src, CLEONOS_O_RDONLY, 0ULL);
    if (src_fd == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: open read failed: %s\n",
                                  "install2disk: 打开读取失败 (open read failed): %s\n"),
                     src);
        return 0;
    }

    if (cleonos_sys_fs_write(dst, "", 0ULL) == 0ULL) {
        (void)cleonos_sys_fd_close(src_fd);
        (void)printf(INSTALL_TEXT("install2disk: create failed: %s\n",
                                  "install2disk: 创建失败 (create failed): %s\n"),
                     dst);
        return 0;
    }

    for (;;) {
        got = cleonos_sys_fd_read(src_fd, buffer, (u64)sizeof(buffer));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(src_fd);
            (void)printf(INSTALL_TEXT("install2disk: read failed: %s\n",
                                      "install2disk: 读取失败 (read failed): %s\n"),
                         src);
            (void)printf(INSTALL_TEXT("install2disk: src=%s mode=stream offset=%llu want=%llu got=%llu size=%llu\n",
                                      "install2disk: 源 (src)=%s 模式 (mode)=stream 偏移 (offset)=%llu 期望 (want)=%llu 得到 (got)=%llu 大小 (size)=%llu\n"),
                         src,
                         (unsigned long long)file_offset, (unsigned long long)sizeof(buffer),
                         (unsigned long long)got, (unsigned long long)size);
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        wrote = cleonos_sys_fs_append(dst, buffer, got);
        if (wrote != got) {
            (void)cleonos_sys_fd_close(src_fd);
            (void)printf(INSTALL_TEXT("install2disk: write failed: %s\n",
                                      "install2disk: 写入失败 (write failed): %s\n"),
                         dst);
            (void)printf(INSTALL_TEXT("install2disk: src=%s offset=%llu chunk=%llu wrote=%llu size=%llu\n",
                                      "install2disk: 源 (src)=%s 偏移 (offset)=%llu 块 (chunk)=%llu 写入 (wrote)=%llu 大小 (size)=%llu\n"),
                         src,
                         (unsigned long long)file_offset, (unsigned long long)got, (unsigned long long)wrote,
                         (unsigned long long)size);
            return 0;
        }

        file_offset += got;
        if (copied_bytes != (u64 *)0) {
            *copied_bytes += got;
        }
        install_progress_add_done_bytes(progress, got);
    }

    (void)cleonos_sys_fd_close(src_fd);

    if (copied_files != (u64 *)0) {
        *copied_files += 1ULL;
    }
    install_progress_finish_item(progress);

    if (cleonos_sys_fs_stat_size(dst) != size) {
        (void)printf(INSTALL_TEXT("install2disk: verify failed: %s\n",
                                  "install2disk: 验证失败 (verify failed): %s\n"),
                     dst);
        (void)printf(INSTALL_TEXT("install2disk: expected=%llu actual=%llu\n",
                                  "install2disk: 期望 (expected)=%llu 实际 (actual)=%llu\n"),
                     (unsigned long long)size,
                     (unsigned long long)cleonos_sys_fs_stat_size(dst));
        return 0;
    }

    return 1;
}

static int install_overwrite_file_whole(const char *src, const char *dst, u64 *copied_files, u64 *copied_bytes,
                                        install_progress *progress) {
    char *data;
    u64 size;
    u64 dst_type;
    u64 fd;
    u64 off = 0ULL;
    u64 wrote;

    size = cleonos_sys_fs_stat_size(src);
    if (size == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: stat failed: %s\n",
                                  "install2disk: 获取文件信息失败 (stat failed): %s\n"),
                     src);
        return 0;
    }

    dst_type = cleonos_sys_fs_stat_type(dst);
    if (dst_type == 2ULL) {
        (void)printf(INSTALL_TEXT("install2disk: target is directory: %s\n",
                                  "install2disk: 目标是目录 (target is directory): %s\n"),
                     dst);
        return 0;
    }

    if (size == 0ULL) {
        wrote = cleonos_sys_fs_write(dst, "", 0ULL);
        if (wrote != 0ULL || cleonos_sys_fs_stat_size(dst) != 0ULL) {
            (void)printf(INSTALL_TEXT("install2disk: overwrite failed: %s\n",
                                      "install2disk: 覆盖失败 (overwrite failed): %s\n"),
                         dst);
            return 0;
        }

        if (copied_files != (u64 *)0) {
            *copied_files += 1ULL;
        }
        install_progress_finish_item(progress);
        return 1;
    }

    data = (char *)malloc((size_t)size);
    if (data == (char *)0) {
        (void)printf(INSTALL_TEXT("install2disk: overwrite buffer allocation failed: %s\n",
                                  "install2disk: 覆盖缓冲区分配失败 (overwrite buffer allocation failed): %s\n"),
                     src);
        return 0;
    }

    fd = cleonos_sys_fd_open(src, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        free(data);
        (void)printf(INSTALL_TEXT("install2disk: open read failed: %s\n",
                                  "install2disk: 打开读取失败 (open read failed): %s\n"),
                     src);
        return 0;
    }

    while (off < size) {
        u64 got = cleonos_sys_fd_read(fd, data + (size_t)off, size - off);
        if (got == (u64)-1 || got == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            free(data);
            (void)printf(INSTALL_TEXT("install2disk: read failed: %s\n",
                                      "install2disk: 读取失败 (read failed): %s\n"),
                         src);
            return 0;
        }
        off += got;
    }
    (void)cleonos_sys_fd_close(fd);

    wrote = cleonos_sys_fs_write(dst, data, size);
    free(data);
    if (wrote != size) {
        (void)printf(INSTALL_TEXT("install2disk: overwrite failed: %s\n",
                                  "install2disk: 覆盖失败 (overwrite failed): %s\n"),
                     dst);
        (void)printf(INSTALL_TEXT("install2disk: src=%s size=%llu wrote=%llu\n",
                                  "install2disk: 源 (src)=%s 大小 (size)=%llu 写入 (wrote)=%llu\n"),
                     src, (unsigned long long)size,
                     (unsigned long long)wrote);
        return 0;
    }

    if (cleonos_sys_fs_stat_size(dst) != size) {
        (void)printf(INSTALL_TEXT("install2disk: verify failed: %s\n",
                                  "install2disk: 验证失败 (verify failed): %s\n"),
                     dst);
        (void)printf(INSTALL_TEXT("install2disk: expected=%llu actual=%llu\n",
                                  "install2disk: 期望 (expected)=%llu 实际 (actual)=%llu\n"),
                     (unsigned long long)size,
                     (unsigned long long)cleonos_sys_fs_stat_size(dst));
        return 0;
    }

    if (copied_bytes != (u64 *)0) {
        *copied_bytes += size;
    }
    if (copied_files != (u64 *)0) {
        *copied_files += 1ULL;
    }
    install_progress_add_done_bytes(progress, size);
    install_progress_finish_item(progress);
    return 1;
}

static int install_scan_tree(const char *src, u64 depth, install_progress *progress) {
    u64 type;
    u64 count;
    u64 i;

    if (progress == (install_progress *)0) {
        return 0;
    }

    if (depth > 32ULL) {
        (void)printf(INSTALL_TEXT("install2disk: directory depth too large while scanning: %s\n",
                                  "install2disk: 扫描时目录深度过大 (directory depth too large): %s\n"),
                     src);
        return 0;
    }

    if (install_path_is_under(src, INSTALL_SKIP_PREFIX) != 0) {
        return 1;
    }

    type = cleonos_sys_fs_stat_type(src);
    if (type == 1ULL) {
        install_progress_plan_file(progress, src, 1ULL);
        return 1;
    }

    if (type != 2ULL) {
        return 1;
    }

    if (strcmp(src, "/proc") == 0 || strcmp(src, "/dev") == 0) {
        return 1;
    }

    count = cleonos_sys_fs_child_count(src);
    if (count == (u64)-1) {
        return 0;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_src[USH_PATH_MAX];

        name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(src, i, name) == 0ULL || name[0] == '\0') {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        if (strcmp(src, "/") == 0 && (strcmp(name, "proc") == 0 || strcmp(name, "dev") == 0)) {
            continue;
        }

        if (install_path_join(child_src, (u64)sizeof(child_src), src, name) == 0) {
            (void)printf(INSTALL_TEXT("install2disk: path too long while scanning near %s\n",
                                      "install2disk: 扫描时路径过长 (path too long) near %s\n"),
                         name);
            return 0;
        }

        if (install_scan_tree(child_src, depth + 1ULL, progress) == 0) {
            return 0;
        }
    }

    return 1;
}

static int install_copy_tree(const char *src, const char *dst, u64 depth, u64 *copied_files, u64 *copied_bytes,
                             install_progress *progress) {
    u64 type;
    u64 count;
    u64 i;

    if (depth > 32ULL) {
        (void)printf(INSTALL_TEXT("install2disk: directory depth too large: %s\n",
                                  "install2disk: 目录深度过大 (directory depth too large): %s\n"),
                     src);
        return 0;
    }

    if (install_path_is_under(src, INSTALL_SKIP_PREFIX) != 0) {
        return 1;
    }

    type = cleonos_sys_fs_stat_type(src);
    if (type == 1ULL) {
        return install_copy_file(src, dst, copied_files, copied_bytes, progress);
    }

    if (type != 2ULL) {
        return 1;
    }

    if (strcmp(src, "/proc") == 0 || strcmp(src, "/dev") == 0) {
        return 1;
    }

    if (install_mkdir(dst) == 0) {
        return 0;
    }

    count = cleonos_sys_fs_child_count(src);
    if (count == (u64)-1) {
        return 0;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_src[USH_PATH_MAX];
        char child_dst[USH_PATH_MAX];

        name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(src, i, name) == 0ULL || name[0] == '\0') {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        if (strcmp(src, "/") == 0 && (strcmp(name, "proc") == 0 || strcmp(name, "dev") == 0)) {
            continue;
        }

        if (install_path_join(child_src, (u64)sizeof(child_src), src, name) == 0 ||
            install_path_join(child_dst, (u64)sizeof(child_dst), dst, name) == 0) {
            (void)printf(INSTALL_TEXT("install2disk: path too long near %s\n",
                                      "install2disk: 路径过长 (path too long) near %s\n"),
                         name);
            return 0;
        }

        if (install_copy_tree(child_src, child_dst, depth + 1ULL, copied_files, copied_bytes, progress) == 0) {
            return 0;
        }
    }

    return 1;
}

static int install_root_join(char *out, u64 out_size, const char *root_path, const char *logical_path) {
    if (out == (char *)0 || out_size == 0ULL || root_path == (const char *)0 || logical_path == (const char *)0 ||
        logical_path[0] != '/') {
        return 0;
    }

    if (strcmp(root_path, "/") == 0) {
        return ((u64)snprintf(out, (unsigned long)out_size, "%s", logical_path) < out_size) ? 1 : 0;
    }

    return ((u64)snprintf(out, (unsigned long)out_size, "%s%s", root_path, logical_path) < out_size) ? 1 : 0;
}

static int install_manifest_path_included(const char *logical_path) {
    if (logical_path == (const char *)0 || logical_path[0] != '/') {
        return 0;
    }

    if (strcmp(logical_path, INSTALL_MANIFEST_SOURCE) == 0 ||
        strcmp(logical_path, "/system/install_manifest.new") == 0 ||
        strcmp(logical_path, "/system/install_manifest.prev") == 0 ||
        strcmp(logical_path, "/system/update_state.db") == 0 || strcmp(logical_path, "/system/users.db") == 0 ||
        strcmp(logical_path, "/kernel.elf") == 0) {
        return 0;
    }

    if (install_path_is_under(logical_path, "/system/pkg") != 0 ||
        install_path_is_under(logical_path, "/home") != 0 || install_path_is_under(logical_path, "/temp") != 0 ||
        install_path_is_under(logical_path, "/dev") != 0 || install_path_is_under(logical_path, "/proc") != 0) {
        return 0;
    }

    if (install_path_is_under(logical_path, "/boot") != 0 || install_path_is_under(logical_path, "/system") != 0 ||
        install_path_is_under(logical_path, "/shell") != 0 || install_path_is_under(logical_path, "/driver") != 0 ||
        install_path_is_under(logical_path, "/limine") != 0) {
        return 1;
    }

    if (strcmp(logical_path, "/limine.conf") == 0 || strcmp(logical_path, "/limine-bios.sys") == 0) {
        return 1;
    }

    return 0;
}

static int install_update_shell_path_included(const char *logical_path) {
    if (logical_path == (const char *)0 || logical_path[0] != '/') {
        return 0;
    }

    if (strcmp(logical_path, "/shell/install2disk.elf") == 0) {
        return 0;
    }

    if (install_manifest_path_included(logical_path) == 0) {
        return 0;
    }

    if (install_path_is_under(logical_path, "/shell") != 0 || install_path_is_under(logical_path, "/driver") != 0 ||
        install_path_is_under(logical_path, "/system") != 0) {
        return 1;
    }

    return 0;
}

static int install_manifest_append_entry(const char *manifest_path, const char *real_path, const char *logical_path,
                                         install_manifest_writer *writer) {
    char hash[INSTALL_SHA256_HEX_LEN + 1U];
    char line[USH_PATH_MAX + INSTALL_SHA256_HEX_LEN + 64U];
    u64 size;
    u64 line_len;

    if (manifest_path == (const char *)0 || real_path == (const char *)0 || logical_path == (const char *)0 ||
        writer == (install_manifest_writer *)0) {
        return 0;
    }

    size = cleonos_sys_fs_stat_size(real_path);
    if (size == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: manifest stat failed: %s\n",
                                  "install2disk: manifest 获取文件信息失败 (manifest stat failed): %s\n"),
                     logical_path);
        return 0;
    }

    if (install_sha256_file_hex(real_path, hash) == 0) {
        (void)printf(INSTALL_TEXT("install2disk: manifest hash failed: %s\n",
                                  "install2disk: manifest 计算哈希失败 (manifest hash failed): %s\n"),
                     logical_path);
        return 0;
    }

    if ((u64)snprintf(line, (unsigned long)sizeof(line), "%s|%llu|%s\n", logical_path, (unsigned long long)size,
                      hash) >= (u64)sizeof(line)) {
        (void)printf(INSTALL_TEXT("install2disk: manifest line too long: %s\n",
                                  "install2disk: manifest 行过长 (line too long): %s\n"),
                     logical_path);
        return 0;
    }

    line_len = (u64)strlen(line);
    if (writer->manifest_bytes + line_len > INSTALL_MANIFEST_MAX_BYTES) {
        install_puts_i18n("install2disk: manifest too large", "install2disk: manifest 过大 (manifest too large)");
        return 0;
    }

    if (cleonos_sys_fs_append(manifest_path, line, line_len) != line_len) {
        (void)printf(INSTALL_TEXT("install2disk: manifest append failed: %s\n",
                                  "install2disk: manifest 追加失败 (append failed): %s\n"),
                     logical_path);
        return 0;
    }

    writer->files++;
    writer->file_bytes += size;
    writer->manifest_bytes += line_len;
    return 1;
}

static int install_manifest_scan_tree_filtered(const char *real_path, const char *logical_path,
                                               install_manifest_writer *writer, u64 depth,
                                               const char *manifest_path,
                                               install_manifest_include_fn include_fn) {
    u64 type;
    u64 count;
    u64 i;

    if (real_path == (const char *)0 || logical_path == (const char *)0 || writer == (install_manifest_writer *)0 ||
        manifest_path == (const char *)0 || include_fn == (install_manifest_include_fn)0) {
        return 0;
    }

    if (depth > 32ULL) {
        (void)printf(INSTALL_TEXT("install2disk: manifest directory depth too large: %s\n",
                                  "install2disk: manifest 目录深度过大 (directory depth too large): %s\n"),
                     logical_path);
        return 0;
    }

    if (strcmp(logical_path, "/proc") == 0 || install_path_is_under(logical_path, "/proc") != 0 ||
        strcmp(logical_path, "/dev") == 0 || install_path_is_under(logical_path, "/dev") != 0 ||
        strcmp(logical_path, "/temp") == 0 || install_path_is_under(logical_path, "/temp") != 0 ||
        strcmp(logical_path, "/home") == 0 || install_path_is_under(logical_path, "/home") != 0 ||
        strcmp(logical_path, "/system/pkg") == 0 || install_path_is_under(logical_path, "/system/pkg") != 0) {
        return 1;
    }

    type = cleonos_sys_fs_stat_type(real_path);
    if (type == 1ULL) {
        if (include_fn(logical_path) == 0) {
            return 1;
        }
        return install_manifest_append_entry(manifest_path, real_path, logical_path, writer);
    }

    if (type != 2ULL) {
        return 1;
    }

    count = cleonos_sys_fs_child_count(real_path);
    if (count == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: manifest list failed: %s\n",
                                  "install2disk: manifest 列目录失败 (list failed): %s\n"),
                     logical_path);
        return 0;
    }

    for (i = 0ULL; i < count; i++) {
        char name[CLEONOS_FS_NAME_MAX];
        char child_real[USH_PATH_MAX];
        char child_logical[USH_PATH_MAX];

        name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(real_path, i, name) == 0ULL || name[0] == '\0') {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        if (install_path_join(child_real, (u64)sizeof(child_real), real_path, name) == 0 ||
            install_path_join(child_logical, (u64)sizeof(child_logical), logical_path, name) == 0) {
            (void)printf(INSTALL_TEXT("install2disk: manifest path too long near %s\n",
                                      "install2disk: manifest 路径过长 (path too long) near %s\n"),
                         name);
            return 0;
        }

        if (install_manifest_scan_tree_filtered(child_real, child_logical, writer, depth + 1ULL, manifest_path,
                                                include_fn) == 0) {
            return 0;
        }
    }

    return 1;
}

static int install_generate_manifest_to(const char *root_path, const char *manifest_path,
                                        install_manifest_include_fn include_fn) {
    char system_path[USH_PATH_MAX];
    install_manifest_writer writer;

    if (root_path == (const char *)0 || root_path[0] != '/' || manifest_path == (const char *)0 ||
        manifest_path[0] != '/' || include_fn == (install_manifest_include_fn)0) {
        return 0;
    }

    if (install_root_join(system_path, (u64)sizeof(system_path), root_path, "/system") == 0) {
        install_puts_i18n("install2disk: manifest path too long",
                          "install2disk: manifest 路径过长 (manifest path too long)");
        return 0;
    }

    if (strcmp(root_path, "/") != 0 && install_mkdir(system_path) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_stat_type(manifest_path) == 1ULL && cleonos_sys_fs_remove(manifest_path) == 0ULL) {
        install_puts_i18n("install2disk: remove old manifest failed",
                          "install2disk: 删除旧 manifest 失败 (remove old manifest failed)");
        return 0;
    }

    if (cleonos_sys_fs_write(manifest_path, "", 0ULL) == 0ULL) {
        install_puts_i18n("install2disk: create manifest failed",
                          "install2disk: 创建 manifest 失败 (create manifest failed)");
        return 0;
    }

    memset(&writer, 0, sizeof(writer));
    if (install_manifest_scan_tree_filtered(root_path, "/", &writer, 0ULL, manifest_path, include_fn) == 0) {
        return 0;
    }

    if (writer.files == 0ULL) {
        install_puts_i18n("install2disk: manifest has no files",
                          "install2disk: manifest 没有文件 (manifest has no files)");
        return 0;
    }

    (void)printf(INSTALL_TEXT("install2disk: manifest generated: %llu files, %llu bytes, %llu manifest bytes\n",
                              "install2disk: manifest 已生成 (generated): %llu 文件 (files), %llu 字节 (bytes), %llu manifest 字节 (manifest bytes)\n"),
                 (unsigned long long)writer.files, (unsigned long long)writer.file_bytes,
                 (unsigned long long)writer.manifest_bytes);
    return 1;
}

static int install_generate_manifest(const char *root_path) {
    char manifest_path[USH_PATH_MAX];

    if (install_root_join(manifest_path, (u64)sizeof(manifest_path), root_path, INSTALL_MANIFEST_SOURCE) == 0) {
        install_puts_i18n("install2disk: manifest path too long",
                          "install2disk: manifest 路径过长 (manifest path too long)");
        return 0;
    }

    return install_generate_manifest_to(root_path, manifest_path, install_manifest_path_included);
}

static int install_manifest_sha_hex_valid(const char *hash) {
    u64 i;

    if (hash == (const char *)0 || strlen(hash) != INSTALL_SHA256_HEX_LEN) {
        return 0;
    }

    for (i = 0ULL; i < INSTALL_SHA256_HEX_LEN; i++) {
        char ch = hash[i];

        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
            continue;
        }

        return 0;
    }

    return 1;
}

static int install_manifest_parse_line(char *line, install_manifest_entry *entry) {
    char *size_field;
    char *hash_field;
    char *end_ptr;
    u64 size_value;

    if (line == (char *)0 || entry == (install_manifest_entry *)0 || line[0] == '\0') {
        return 0;
    }

    size_field = strchr(line, '|');
    if (size_field == (char *)0) {
        return 0;
    }
    *size_field = '\0';
    size_field++;

    hash_field = strchr(size_field, '|');
    if (hash_field == (char *)0) {
        return 0;
    }
    *hash_field = '\0';
    hash_field++;

    if (line[0] != '/' || strlen(line) >= sizeof(entry->path) || install_manifest_sha_hex_valid(hash_field) == 0) {
        return 0;
    }

    end_ptr = (char *)0;
    size_value = (u64)strtoull(size_field, &end_ptr, 10);
    if (end_ptr == size_field || end_ptr == (char *)0 || *end_ptr != '\0') {
        return 0;
    }

    (void)snprintf(entry->path, (unsigned long)sizeof(entry->path), "%s", line);
    entry->size = size_value;
    (void)snprintf(entry->hash, (unsigned long)sizeof(entry->hash), "%s", hash_field);
    return 1;
}

static u64 install_manifest_load(const char *manifest_path, install_manifest_entry *entries, u64 max_entries,
                                 int missing_ok) {
    char *buffer;
    u64 size;
    u64 fd;
    u64 off = 0ULL;
    u64 line_start = 0ULL;
    u64 count = 0ULL;
    u64 i;

    if (manifest_path == (const char *)0 || entries == (install_manifest_entry *)0 || max_entries == 0ULL) {
        return (u64)-1;
    }

    size = cleonos_sys_fs_stat_size(manifest_path);
    if (size == (u64)-1) {
        if (missing_ok != 0) {
            return 0ULL;
        }
        (void)printf(INSTALL_TEXT("install2disk: manifest missing: %s\n",
                                  "install2disk: manifest 缺失 (missing): %s\n"),
                     manifest_path);
        return (u64)-1;
    }

    if (size > INSTALL_MANIFEST_MAX_BYTES) {
        (void)printf(INSTALL_TEXT("install2disk: manifest too large: %s\n",
                                  "install2disk: manifest 过大 (too large): %s\n"),
                     manifest_path);
        return (u64)-1;
    }

    buffer = (char *)malloc((size_t)size + 1U);
    if (buffer == (char *)0) {
        install_puts_i18n("install2disk: manifest buffer allocation failed",
                          "install2disk: manifest 缓冲区分配失败 (buffer allocation failed)");
        return (u64)-1;
    }

    fd = cleonos_sys_fd_open(manifest_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        free(buffer);
        (void)printf(INSTALL_TEXT("install2disk: manifest open failed: %s\n",
                                  "install2disk: manifest 打开失败 (open failed): %s\n"),
                     manifest_path);
        return (u64)-1;
    }

    while (off < size) {
        u64 got = cleonos_sys_fd_read(fd, buffer + (size_t)off, size - off);
        if (got == (u64)-1 || got == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            free(buffer);
            (void)printf(INSTALL_TEXT("install2disk: manifest read failed: %s\n",
                                      "install2disk: manifest 读取失败 (read failed): %s\n"),
                         manifest_path);
            return (u64)-1;
        }
        off += got;
    }
    (void)cleonos_sys_fd_close(fd);
    buffer[size] = '\0';

    for (i = 0ULL; i <= size; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0') {
            char *line = buffer + (size_t)line_start;
            buffer[i] = '\0';
            if (i > line_start && buffer[i - 1ULL] == '\r') {
                buffer[i - 1ULL] = '\0';
            }
            if (line[0] != '\0') {
                if (count >= max_entries) {
                    free(buffer);
                    install_puts_i18n("install2disk: manifest entry limit exceeded",
                                      "install2disk: manifest 条目数量超限 (entry limit exceeded)");
                    return (u64)-1;
                }
                if (install_manifest_parse_line(line, entries + count) == 0) {
                    free(buffer);
                    (void)printf(INSTALL_TEXT("install2disk: invalid manifest entry: %s\n",
                                              "install2disk: 无效 manifest 条目 (invalid manifest entry): %s\n"),
                                 line);
                    return (u64)-1;
                }
                count++;
            }
            line_start = i + 1ULL;
        }
    }

    free(buffer);
    return count;
}

static install_manifest_entry *install_manifest_find(install_manifest_entry *entries, u64 count, const char *path) {
    u64 i;

    if (entries == (install_manifest_entry *)0 || path == (const char *)0) {
        return (install_manifest_entry *)0;
    }

    for (i = 0ULL; i < count; i++) {
        if (strcmp(entries[i].path, path) == 0) {
            return entries + i;
        }
    }

    return (install_manifest_entry *)0;
}

static int install_manifest_entry_same(const install_manifest_entry *a, const install_manifest_entry *b) {
    if (a == (const install_manifest_entry *)0 || b == (const install_manifest_entry *)0) {
        return 0;
    }

    return (a->size == b->size && strcmp(a->hash, b->hash) == 0) ? 1 : 0;
}

static int install_verify_file_required(install_verify_result *result, const char *logical_path) {
    char real_path[USH_PATH_MAX];
    u64 type;
    u64 size;

    if (result == (install_verify_result *)0 || logical_path == (const char *)0) {
        return 0;
    }

    result->required_checked++;
    if (install_root_join(real_path, (u64)sizeof(real_path), result->root_path, logical_path) == 0) {
        (void)printf(INSTALL_TEXT("install2disk verify: path too long: %s\n",
                                  "install2disk verify: 路径过长 (path too long): %s\n"),
                     logical_path);
        result->corrupt++;
        return 0;
    }

    type = cleonos_sys_fs_stat_type(real_path);
    if (type != 1ULL) {
        (void)printf(INSTALL_TEXT("install2disk verify: missing required file: %s\n",
                                  "install2disk verify: 缺少必需文件 (missing required file): %s\n"),
                     logical_path);
        result->missing++;
        return 0;
    }

    size = cleonos_sys_fs_stat_size(real_path);
    if (size == (u64)-1 || size == 0ULL) {
        (void)printf(INSTALL_TEXT("install2disk verify: invalid required file size: %s\n",
                                  "install2disk verify: 必需文件大小无效 (invalid required file size): %s\n"),
                     logical_path);
        result->mismatches++;
        return 0;
    }

    return 1;
}

static int install_verify_boot_sectors(install_verify_result *result) {
    static unsigned char sector[INSTALL_SECTOR_SIZE];
    u64 stage2_loc;
    u64 i;
    int nonzero = 0;
    int ok = 1;

    if (result == (install_verify_result *)0) {
        return 0;
    }

    if (cleonos_sys_disk_read_sector(0ULL, sector) == 0ULL) {
        install_puts_i18n("install2disk verify: cannot read MBR sector",
                          "install2disk verify: 无法读取 MBR 扇区 (cannot read MBR sector)");
        result->boot_errors++;
        return 0;
    }

    if (sector[510U] != 0x55U || sector[511U] != 0xAAU) {
        install_puts_i18n("install2disk verify: MBR signature missing",
                          "install2disk verify: MBR 签名缺失 (MBR signature missing)");
        result->boot_errors++;
        ok = 0;
    }

    stage2_loc = 0ULL;
    for (i = 0ULL; i < 8ULL; i++) {
        stage2_loc |= ((u64)sector[0x1A4U + i]) << (8ULL * i);
    }

    if (stage2_loc != INSTALL_STAGE2_LBA * INSTALL_SECTOR_SIZE) {
        (void)printf(INSTALL_TEXT("install2disk verify: unexpected Limine stage2 pointer: %llu\n",
                                  "install2disk verify: Limine stage2 指针异常 (unexpected pointer): %llu\n"),
                     (unsigned long long)stage2_loc);
        result->boot_errors++;
        ok = 0;
    }

    if (cleonos_sys_disk_read_sector(INSTALL_STAGE2_LBA, sector) == 0ULL) {
        install_puts_i18n("install2disk verify: cannot read Limine stage2 sector",
                          "install2disk verify: 无法读取 Limine stage2 扇区 (cannot read Limine stage2 sector)");
        result->boot_errors++;
        return 0;
    }

    for (i = 0ULL; i < INSTALL_SECTOR_SIZE; i++) {
        if (sector[i] != 0U) {
            nonzero = 1;
            break;
        }
    }

    if (nonzero == 0) {
        install_puts_i18n("install2disk verify: Limine stage2 sector is blank",
                          "install2disk verify: Limine stage2 扇区为空 (sector is blank)");
        result->boot_errors++;
        ok = 0;
    }

    return ok;
}

static int install_verify_manifest_entry(install_verify_result *result, char *line) {
    char *size_field;
    char *hash_field;
    char *end_ptr;
    char real_path[USH_PATH_MAX];
    char actual_hash[INSTALL_SHA256_HEX_LEN + 1U];
    u64 expected_size;
    u64 actual_size;

    if (result == (install_verify_result *)0 || line == (char *)0 || line[0] == '\0') {
        return 1;
    }

    size_field = strchr(line, '|');
    if (size_field == (char *)0) {
        (void)printf(INSTALL_TEXT("install2disk verify: invalid manifest line: %s\n",
                                  "install2disk verify: 无效 manifest 行 (invalid manifest line): %s\n"),
                     line);
        result->corrupt++;
        return 0;
    }
    *size_field = '\0';
    size_field++;

    hash_field = strchr(size_field, '|');
    if (hash_field == (char *)0) {
        (void)printf(INSTALL_TEXT("install2disk verify: invalid manifest line: %s\n",
                                  "install2disk verify: 无效 manifest 行 (invalid manifest line): %s\n"),
                     line);
        result->corrupt++;
        return 0;
    }
    *hash_field = '\0';
    hash_field++;

    if (line[0] != '/' || install_manifest_sha_hex_valid(hash_field) == 0) {
        (void)printf(INSTALL_TEXT("install2disk verify: invalid manifest entry: %s\n",
                                  "install2disk verify: 无效 manifest 条目 (invalid manifest entry): %s\n"),
                     line);
        result->corrupt++;
        return 0;
    }

    end_ptr = (char *)0;
    expected_size = (u64)strtoull(size_field, &end_ptr, 10);
    if (end_ptr == size_field || end_ptr == (char *)0 || *end_ptr != '\0') {
        (void)printf(INSTALL_TEXT("install2disk verify: invalid manifest size: %s\n",
                                  "install2disk verify: 无效 manifest 大小 (invalid manifest size): %s\n"),
                     line);
        result->corrupt++;
        return 0;
    }

    result->manifest_entries++;
    if (install_root_join(real_path, (u64)sizeof(real_path), result->root_path, line) == 0) {
        (void)printf(INSTALL_TEXT("install2disk verify: path too long: %s\n",
                                  "install2disk verify: 路径过长 (path too long): %s\n"),
                     line);
        result->corrupt++;
        return 0;
    }

    if (cleonos_sys_fs_stat_type(real_path) != 1ULL) {
        (void)printf(INSTALL_TEXT("install2disk verify: missing manifest file: %s\n",
                                  "install2disk verify: 缺少 manifest 文件 (missing manifest file): %s\n"),
                     line);
        result->missing++;
        return 0;
    }

    actual_size = cleonos_sys_fs_stat_size(real_path);
    if (actual_size != expected_size) {
        (void)printf(INSTALL_TEXT("install2disk verify: size mismatch: %s expected=%llu actual=%llu\n",
                                  "install2disk verify: 大小不匹配 (size mismatch): %s expected=%llu actual=%llu\n"),
                     line,
                     (unsigned long long)expected_size, (unsigned long long)actual_size);
        result->mismatches++;
        return 0;
    }

    if (install_sha256_file_hex(real_path, actual_hash) == 0) {
        (void)printf(INSTALL_TEXT("install2disk verify: hash read failed: %s\n",
                                  "install2disk verify: 哈希读取失败 (hash read failed): %s\n"),
                     line);
        result->corrupt++;
        return 0;
    }

    if (strcmp(actual_hash, hash_field) != 0) {
        (void)printf(INSTALL_TEXT("install2disk verify: sha256 mismatch: %s\n",
                                  "install2disk verify: SHA-256 不匹配 (sha256 mismatch): %s\n"),
                     line);
        result->mismatches++;
        return 0;
    }

    result->files_checked++;
    result->bytes_checked += actual_size;
    return 1;
}

static int install_verify_manifest(install_verify_result *result) {
    char manifest_path[USH_PATH_MAX];
    char *buffer;
    u64 size;
    u64 fd;
    u64 off = 0ULL;
    u64 line_start = 0ULL;
    u64 i;
    int ok = 1;

    if (result == (install_verify_result *)0) {
        return 0;
    }

    if (install_root_join(manifest_path, (u64)sizeof(manifest_path), result->root_path, INSTALL_MANIFEST_SOURCE) ==
        0) {
        install_puts_i18n("install2disk verify: manifest path too long",
                          "install2disk verify: manifest 路径过长 (manifest path too long)");
        result->corrupt++;
        return 0;
    }

    size = cleonos_sys_fs_stat_size(manifest_path);
    if (size == (u64)-1) {
        install_puts_i18n("install2disk verify: missing /system/install_manifest.db",
                          "install2disk verify: 缺少 /system/install_manifest.db (missing)");
        result->missing++;
        return 0;
    }

    if (size > INSTALL_MANIFEST_MAX_BYTES) {
        install_puts_i18n("install2disk verify: manifest too large",
                          "install2disk verify: manifest 过大 (manifest too large)");
        result->corrupt++;
        return 0;
    }

    buffer = (char *)malloc((size_t)size + 1U);
    if (buffer == (char *)0) {
        install_puts_i18n("install2disk verify: manifest buffer allocation failed",
                          "install2disk verify: manifest 缓冲区分配失败 (buffer allocation failed)");
        result->corrupt++;
        return 0;
    }

    fd = cleonos_sys_fd_open(manifest_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        free(buffer);
        install_puts_i18n("install2disk verify: manifest open failed",
                          "install2disk verify: manifest 打开失败 (open failed)");
        result->corrupt++;
        return 0;
    }

    while (off < size) {
        u64 got = cleonos_sys_fd_read(fd, buffer + (size_t)off, size - off);
        if (got == (u64)-1 || got == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            free(buffer);
            install_puts_i18n("install2disk verify: manifest read failed",
                              "install2disk verify: manifest 读取失败 (read failed)");
            result->corrupt++;
            return 0;
        }
        off += got;
    }
    (void)cleonos_sys_fd_close(fd);
    buffer[size] = '\0';

    for (i = 0ULL; i <= size; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0') {
            char *line = buffer + (size_t)line_start;
            buffer[i] = '\0';
            if (i > line_start && buffer[i - 1ULL] == '\r') {
                buffer[i - 1ULL] = '\0';
            }
            if (line[0] != '\0' && install_verify_manifest_entry(result, line) == 0) {
                ok = 0;
            }
            line_start = i + 1ULL;
        }
    }

    free(buffer);
    return ok;
}

static int install_verify_run(void) {
    static const char *required_files[] = {"/kernel.elf",
                                           "/boot/limine/limine-bios.sys",
                                           "/boot/limine-bios.sys",
                                           "/limine/limine-bios.sys",
                                           "/limine-bios.sys",
                                           "/boot/limine/limine.conf",
                                           "/boot/limine.conf",
                                           "/limine/limine.conf",
                                           "/limine.conf",
                                           "/system/users.db",
                                           "/system/tty.psf",
                                           "/shell/shell.elf"};
    install_verify_result result;
    char mount_path[USH_PATH_MAX];
    u64 i;

    if (cleonos_sys_disk_present() == 0ULL) {
        install_puts_i18n("install2disk verify: disk not present",
                          "install2disk verify: 未检测到硬盘 (disk not present)");
        return 0;
    }

    memset(&result, 0, sizeof(result));
    result.root_path = INSTALL_MOUNT_PATH;
    mount_path[0] = '\0';

    if (cleonos_sys_disk_mounted() != 0ULL && cleonos_sys_disk_mount_path(mount_path, (u64)sizeof(mount_path)) != 0ULL) {
        result.root_path = (strcmp(mount_path, "/") == 0) ? "/" : mount_path;
    } else if (install_mount_existing_disk() == 0) {
        install_puts_i18n("install2disk verify: mount target disk failed",
                          "install2disk verify: 挂载目标硬盘失败 (mount target disk failed)");
        return 0;
    }

    install_stage("verify hard disk install");
    (void)printf(INSTALL_TEXT("install2disk verify: root=%s\n", "install2disk verify: 根目录 (root)=%s\n"),
                 result.root_path);

    (void)install_verify_boot_sectors(&result);

    for (i = 0ULL; i < (u64)(sizeof(required_files) / sizeof(required_files[0])); i++) {
        (void)install_verify_file_required(&result, required_files[i]);
    }

    (void)install_verify_manifest(&result);

    if (result.manifest_entries == 0ULL) {
        install_puts_i18n("install2disk verify: manifest has no entries",
                          "install2disk verify: manifest 没有条目 (manifest has no entries)");
        result.corrupt++;
    }

    (void)printf(INSTALL_TEXT("install2disk verify: required=%llu manifest_entries=%llu files_checked=%llu bytes_checked=%llu\n",
                              "install2disk verify: 必需项 (required)=%llu manifest 条目 (manifest_entries)=%llu 已检查文件 (files_checked)=%llu 已检查字节 (bytes_checked)=%llu\n"),
                 (unsigned long long)result.required_checked, (unsigned long long)result.manifest_entries,
                 (unsigned long long)result.files_checked, (unsigned long long)result.bytes_checked);
    (void)printf(INSTALL_TEXT("install2disk verify: missing=%llu mismatches=%llu corrupt=%llu boot_errors=%llu\n",
                              "install2disk verify: 缺失 (missing)=%llu 不匹配 (mismatches)=%llu 损坏 (corrupt)=%llu 启动错误 (boot_errors)=%llu\n"),
                 (unsigned long long)result.missing, (unsigned long long)result.mismatches,
                 (unsigned long long)result.corrupt, (unsigned long long)result.boot_errors);

    if (result.missing == 0ULL && result.mismatches == 0ULL && result.corrupt == 0ULL && result.boot_errors == 0ULL) {
        install_puts_i18n("install2disk verify: OK", "install2disk verify: 正常 (OK)");
        return 1;
    }

    install_puts_i18n("install2disk verify: FAILED", "install2disk verify: 失败 (FAILED)");
    return 0;
}

static int install_raw_write_bytes(u64 start_lba, const char *src_path, u64 start_offset, u64 byte_count,
                                   install_progress *progress) {
    static char file_buf[4096];
    static unsigned char sector[INSTALL_SECTOR_SIZE];
    u64 fd;
    u64 file_pos = 0ULL;
    u64 written = 0ULL;
    u64 cached_start = (u64)-1;
    u64 cached_len = 0ULL;

    fd = cleonos_sys_fd_open(src_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: open boot blob failed: %s\n",
                                  "install2disk: 打开启动 blob 失败 (open boot blob failed): %s\n"),
                     src_path);
        return 0;
    }

    while (written < byte_count) {
        u64 want_pos = start_offset + written;
        u64 sector_lba = start_lba + (written / INSTALL_SECTOR_SIZE);
        u64 sector_off = written % INSTALL_SECTOR_SIZE;
        u64 chunk = INSTALL_SECTOR_SIZE - sector_off;
        u64 i;

        if (chunk > byte_count - written) {
            chunk = byte_count - written;
        }

        if (cleonos_sys_disk_read_sector(sector_lba, sector) == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            (void)printf(INSTALL_TEXT("install2disk: read sector failed: %llu\n",
                                      "install2disk: 读取扇区失败 (read sector failed): %llu\n"),
                         (unsigned long long)sector_lba);
            return 0;
        }

        for (i = 0ULL; i < chunk; i++) {
            u64 p = want_pos + i;
            if (p < cached_start || p >= cached_start + cached_len) {
                if (p < file_pos) {
                    (void)cleonos_sys_fd_close(fd);
                    install_puts_i18n("install2disk: non-sequential boot blob read",
                                      "install2disk: 启动 blob 非顺序读取 (non-sequential boot blob read)");
                    return 0;
                }

                while (file_pos < p) {
                    u64 skip_need = p - file_pos;
                    u64 got;
                    if (skip_need > (u64)sizeof(file_buf)) {
                        skip_need = (u64)sizeof(file_buf);
                    }
                    got = cleonos_sys_fd_read(fd, file_buf, skip_need);
                    if (got == (u64)-1 || got == 0ULL) {
                        (void)cleonos_sys_fd_close(fd);
                        install_puts_i18n("install2disk: boot blob seek failed",
                                          "install2disk: 启动 blob seek 失败 (boot blob seek failed)");
                        return 0;
                    }
                    file_pos += got;
                }

                cached_start = file_pos;
                cached_len = cleonos_sys_fd_read(fd, file_buf, (u64)sizeof(file_buf));
                if (cached_len == (u64)-1 || cached_len == 0ULL) {
                    (void)cleonos_sys_fd_close(fd);
                    install_puts_i18n("install2disk: boot blob read failed",
                                      "install2disk: 启动 blob 读取失败 (boot blob read failed)");
                    return 0;
                }
                file_pos += cached_len;
            }

            sector[sector_off + i] = (unsigned char)file_buf[p - cached_start];
        }

        if (cleonos_sys_disk_write_sector(sector_lba, sector) == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            (void)printf(INSTALL_TEXT("install2disk: write sector failed: %llu\n",
                                      "install2disk: 写入扇区失败 (write sector failed): %llu\n"),
                         (unsigned long long)sector_lba);
            return 0;
        }

        written += chunk;
        install_progress_add_done_bytes(progress, chunk);
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

static int install_limine_bios(install_progress *progress) {
    static unsigned char mbr[INSTALL_SECTOR_SIZE];
    u64 hdd_size;
    u64 stage2_size;
    u64 stage2_bytes;
    u64 stage2_loc = INSTALL_STAGE2_LBA * INSTALL_SECTOR_SIZE;
    u64 sector_count;
    u64 partition_lba = 2048ULL;
    u64 partition_sectors;
    unsigned int i;

    hdd_size = cleonos_sys_fs_stat_size(INSTALL_LIMINE_HDD_SOURCE);
    stage2_size = cleonos_sys_fs_stat_size(INSTALL_LIMINE_SYS_SOURCE);
    if (hdd_size == (u64)-1 || hdd_size < INSTALL_SECTOR_SIZE || stage2_size == (u64)-1 || stage2_size == 0ULL) {
        install_puts_i18n("install2disk: missing Limine install blobs",
                          "install2disk: 缺少 Limine 安装 blob (missing Limine install blobs)");
        return 0;
    }

    if (install_raw_write_bytes(0ULL, INSTALL_LIMINE_HDD_SOURCE, 0ULL, INSTALL_SECTOR_SIZE, progress) == 0) {
        return 0;
    }

    stage2_bytes = hdd_size - INSTALL_SECTOR_SIZE;
    if (stage2_bytes > 0ULL &&
        install_raw_write_bytes(INSTALL_STAGE2_LBA, INSTALL_LIMINE_HDD_SOURCE, INSTALL_SECTOR_SIZE, stage2_bytes,
                                progress) == 0) {
        return 0;
    }

    if (cleonos_sys_disk_read_sector(0ULL, mbr) == 0ULL) {
        return 0;
    }

    for (i = 0U; i < 8U; i++) {
        mbr[0x1A4U + i] = (unsigned char)((stage2_loc >> (8U * i)) & 0xFFULL);
    }
    for (i = 0x1BEU; i < 0x1FEU; i++) {
        mbr[i] = 0U;
    }

    sector_count = cleonos_sys_disk_sector_count();
    if (sector_count <= partition_lba || sector_count > 0xFFFFFFFFULL) {
        (void)printf(INSTALL_TEXT("install2disk: invalid disk sector count: %llu\n",
                                  "install2disk: 硬盘扇区数量无效 (invalid disk sector count): %llu\n"),
                     (unsigned long long)sector_count);
        return 0;
    }
    partition_sectors = sector_count - partition_lba;

    mbr[0x1BEU] = 0x80U;
    mbr[0x1BEU + 1U] = 0x20U;
    mbr[0x1BEU + 2U] = 0x21U;
    mbr[0x1BEU + 3U] = 0x00U;
    mbr[0x1BEU + 4U] = 0x0CU;
    mbr[0x1BEU + 5U] = 0xFEU;
    mbr[0x1BEU + 6U] = 0xFFU;
    mbr[0x1BEU + 7U] = 0xFFU;
    for (i = 0U; i < 4U; i++) {
        mbr[0x1BEU + 8U + i] = (unsigned char)((partition_lba >> (8U * i)) & 0xFFULL);
        mbr[0x1BEU + 12U + i] = (unsigned char)((partition_sectors >> (8U * i)) & 0xFFULL);
    }

    mbr[510U] = 0x55U;
    mbr[511U] = 0xAAU;

    if (cleonos_sys_disk_write_sector(0ULL, mbr) == 0ULL) {
        return 0;
    }

    install_progress_finish_item(progress);
    return 1;
}

static int install_prepare_boot_files(u64 *copied_files, u64 *copied_bytes, install_progress *progress) {
    if (install_prepare_limine_dirs() == 0) {
        return 0;
    }

    if (install_overwrite_file_whole(INSTALL_KERNEL_SOURCE, INSTALL_KERNEL_TARGET, copied_files, copied_bytes,
                                     progress) == 0) {
        return 0;
    }

    if (install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/boot/limine/limine-bios.sys", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/boot/limine-bios.sys", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/limine/limine-bios.sys", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/limine-bios.sys", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/boot/limine/limine.conf", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/boot/limine.conf", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/limine/limine.conf", copied_files,
                          copied_bytes, progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/limine.conf", copied_files, copied_bytes,
                          progress) == 0) {
        return 0;
    }

    return 1;
}

static void install_read_secret_line(const char *prompt, char *out, u64 out_size) {
    u64 len = 0ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    out[0] = '\0';
    if (prompt != (const char *)0) {
        (void)printf("%s", prompt);
    }

    for (;;) {
        int ch = getchar();

        if (ch == EOF || ch == '\n') {
            putchar('\n');
            out[len] = '\0';
            return;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\b' || ch == 0x7F) {
            if (len > 0ULL) {
                len--;
                out[len] = '\0';
            }
            continue;
        }

        if (len + 1ULL < out_size) {
            out[len++] = (char)ch;
        }
    }
}

static int install_setup_root_account(void) {
    char password[96];
    char confirm[96];
    char hash[CLEONOS_USER_HASH_HEX_LEN + 1U];
    char record[CLEONOS_USER_RECORD_MAX];

    install_stage("setup root account");

    if (install_mkdir(INSTALL_HOME_TARGET) == 0 || install_mkdir(INSTALL_ROOT_HOME_TARGET) == 0) {
        return 0;
    }

    for (;;) {
        install_read_secret_line(INSTALL_TEXT("install2disk: set root password: ",
                                              "install2disk: 设置 root 密码 (set root password): "),
                                 password, (u64)sizeof(password));
        install_read_secret_line(INSTALL_TEXT("install2disk: confirm root password: ",
                                              "install2disk: 确认 root 密码 (confirm root password): "),
                                 confirm, (u64)sizeof(confirm));

        if (password[0] == '\0') {
            install_puts_i18n("install2disk: root password cannot be empty",
                              "install2disk: root 密码不能为空 (root password cannot be empty)");
            continue;
        }

        if (strcmp(password, confirm) != 0) {
            install_puts_i18n("install2disk: passwords do not match",
                              "install2disk: 两次密码不一致 (passwords do not match)");
            continue;
        }

        break;
    }

    cleonos_user_hash_password(password, hash);
    (void)snprintf(record, (unsigned long)sizeof(record), "root:admin:%s:/home/root:0\n", hash);

    if (cleonos_sys_fs_write(INSTALL_USER_DB_TARGET, record, (u64)strlen(record)) == 0ULL) {
        install_puts_i18n("install2disk: failed to create root account database",
                          "install2disk: 创建 root 账户数据库失败 (failed to create root account database)");
        return 0;
    }

    install_puts_i18n("install2disk: root account created", "install2disk: root 账户已创建 (root account created)");
    return 1;
}

static int install_repair_copy_path(const char *src_path, u64 *copied_files, u64 *copied_bytes) {
    char dst_path[USH_PATH_MAX];
    install_progress progress;

    if (src_path == (const char *)0 || src_path[0] != '/') {
        install_puts_i18n("install2disk: repair path must be absolute",
                          "install2disk: 修复路径必须是绝对路径 (repair path must be absolute)");
        return 0;
    }

    if (install_build_target_path(dst_path, (u64)sizeof(dst_path), src_path) == 0) {
        (void)printf(INSTALL_TEXT("install2disk: invalid repair path: %s\n",
                                  "install2disk: 无效修复路径 (invalid repair path): %s\n"),
                     src_path);
        return 0;
    }

    if (cleonos_sys_fs_stat_type(src_path) != 1ULL) {
        (void)printf(INSTALL_TEXT("install2disk: source file missing in ISO/rootfs: %s\n",
                                  "install2disk: ISO/rootfs 中缺少源文件 (source file missing): %s\n"),
                     src_path);
        return 0;
    }

    memset(&progress, 0, sizeof(progress));
    progress.label = "repair file";
    install_progress_plan_file(&progress, src_path, 1ULL);
    install_progress_print(&progress, 1);

    if (install_overwrite_file_whole(src_path, dst_path, copied_files, copied_bytes, &progress) == 0) {
        return 0;
    }

    progress.done_items = progress.total_items;
    progress.done_bytes = progress.total_bytes;
    install_progress_print(&progress, 1);
    (void)printf(INSTALL_TEXT("install2disk: repaired %s -> %s\n",
                              "install2disk: 已修复 (repaired) %s -> %s\n"),
                 src_path, dst_path);
    return 1;
}

static int install_repair_limine_conf(u64 *copied_files, u64 *copied_bytes) {
    install_progress progress;

    memset(&progress, 0, sizeof(progress));
    progress.label = "repair limine config";
    install_progress_plan_file(&progress, INSTALL_LIMINE_CONF_SOURCE, 4ULL);
    install_progress_print(&progress, 1);

    if (install_prepare_limine_dirs() == 0) {
        return 0;
    }

    if (install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/boot/limine/limine.conf", copied_files,
                          copied_bytes, &progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/boot/limine.conf", copied_files,
                          copied_bytes, &progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/limine/limine.conf", copied_files,
                          copied_bytes, &progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/limine.conf", copied_files, copied_bytes,
                          &progress) == 0) {
        return 0;
    }

    progress.done_items = progress.total_items;
    progress.done_bytes = progress.total_bytes;
    install_progress_print(&progress, 1);
    install_puts_i18n("install2disk: Limine config repaired",
                      "install2disk: Limine 配置已修复 (Limine config repaired)");
    return 1;
}

static int install_repair_manifest(void) {
    if (install_generate_manifest(INSTALL_MOUNT_PATH) == 0) {
        return 0;
    }

    install_puts_i18n("install2disk: manifest repaired", "install2disk: manifest 已修复 (manifest repaired)");
    return 1;
}

static int install_repair_limine_sys(u64 *copied_files, u64 *copied_bytes) {
    install_progress progress;

    memset(&progress, 0, sizeof(progress));
    progress.label = "repair limine sys";
    install_progress_plan_file(&progress, INSTALL_LIMINE_SYS_SOURCE, 4ULL);
    install_progress_print(&progress, 1);

    if (install_prepare_limine_dirs() == 0) {
        return 0;
    }

    if (install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/boot/limine/limine-bios.sys", copied_files,
                          copied_bytes, &progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/boot/limine-bios.sys", copied_files,
                          copied_bytes, &progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/limine/limine-bios.sys", copied_files,
                          copied_bytes, &progress) == 0 ||
        install_overwrite_file_whole(INSTALL_LIMINE_SYS_SOURCE, INSTALL_MOUNT_PATH "/limine-bios.sys", copied_files, copied_bytes,
                          &progress) == 0) {
        return 0;
    }

    progress.done_items = progress.total_items;
    progress.done_bytes = progress.total_bytes;
    install_progress_print(&progress, 1);
    install_puts_i18n("install2disk: Limine sys repaired",
                      "install2disk: Limine sys 已修复 (Limine sys repaired)");
    return 1;
}

static int install_repair_bootloader(void) {
    install_progress progress;

    memset(&progress, 0, sizeof(progress));
    progress.label = "repair bootloader";
    install_progress_plan_file(&progress, INSTALL_LIMINE_HDD_SOURCE, 1ULL);
    progress.total_items += 1ULL;
    install_progress_print(&progress, 1);

    if (install_limine_bios(&progress) == 0) {
        return 0;
    }

    progress.done_items = progress.total_items;
    progress.done_bytes = progress.total_bytes;
    install_progress_print(&progress, 1);
    install_puts_i18n("install2disk: BIOS bootloader repaired",
                      "install2disk: BIOS 引导器已修复 (BIOS bootloader repaired)");
    return 1;
}

static int install_repair_limine_full(u64 *copied_files, u64 *copied_bytes) {
    if (install_repair_limine_sys(copied_files, copied_bytes) == 0) {
        return 0;
    }

    if (install_repair_limine_conf(copied_files, copied_bytes) == 0) {
        return 0;
    }

    if (install_repair_bootloader() == 0) {
        return 0;
    }

    install_puts_i18n("install2disk: full Limine BIOS chain repaired",
                      "install2disk: 完整 Limine BIOS 链已修复 (full Limine BIOS chain repaired)");
    return 1;
}

static int install_repair_component(const char *kind, const char *name) {
    char src_path[USH_PATH_MAX];
    u64 copied_files = 0ULL;
    u64 copied_bytes = 0ULL;

    if (kind == (const char *)0 || kind[0] == '\0') {
        return 0;
    }

    if (strcmp(kind, "kernel") == 0) {
        install_puts_i18n("install2disk: kernel update is not a repair target",
                          "install2disk: 内核更新不是 repair 目标 (kernel update is not a repair target)");
        install_puts_i18n("install2disk: use install2disk update-kernel",
                          "install2disk: 请使用 install2disk update-kernel (use update-kernel)");
        return 0;
    }

    install_stage("repair component");

    if (install_mount_existing_disk() == 0) {
        install_puts_i18n("install2disk: mount existing disk failed",
                          "install2disk: 挂载已有硬盘失败 (mount existing disk failed)");
        return 0;
    }

    if (strcmp(kind, "bootloader") == 0 || strcmp(kind, "mbr") == 0) {
        return install_repair_bootloader();
    }

    if (strcmp(kind, "manifest") == 0 || strcmp(kind, "install-manifest") == 0) {
        return install_repair_manifest();
    }

    if (strcmp(kind, "limine-conf") == 0 || strcmp(kind, "limine-config") == 0 || strcmp(kind, "config") == 0) {
        if (install_repair_limine_conf(&copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (strcmp(kind, "limine-sys") == 0 || strcmp(kind, "sys") == 0) {
        if (install_repair_limine_sys(&copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (strcmp(kind, "limine") == 0) {
        if (install_repair_limine_full(&copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (strcmp(kind, "path") == 0) {
        if (name == (const char *)0 || name[0] != '/') {
            install_puts_i18n("install2disk: repair path requires an absolute path",
                              "install2disk: repair path 需要绝对路径 (requires an absolute path)");
            return 0;
        }
        if (install_repair_copy_path(name, &copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (name == (const char *)0 || name[0] == '\0') {
        install_puts_i18n("install2disk: repair component requires a name",
                          "install2disk: repair 组件需要名称 (requires a name)");
        return 0;
    }

    if (strcmp(kind, "shell") == 0 || strcmp(kind, "app") == 0) {
        if (install_build_component_path(src_path, (u64)sizeof(src_path), "/shell", name) == 0) {
            install_puts_i18n("install2disk: invalid shell component name",
                              "install2disk: 无效 shell 组件名 (invalid shell component name)");
            return 0;
        }
        if (install_repair_copy_path(src_path, &copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (strcmp(kind, "uwm") == 0) {
        if (install_build_component_path(src_path, (u64)sizeof(src_path), "/shell/uwm", name) == 0) {
            install_puts_i18n("install2disk: invalid uwm component name",
                              "install2disk: 无效 uwm 组件名 (invalid uwm component name)");
            return 0;
        }
        if (install_repair_copy_path(src_path, &copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (strcmp(kind, "driver") == 0 || strcmp(kind, "drv") == 0) {
        if (install_build_component_path(src_path, (u64)sizeof(src_path), "/driver", name) == 0) {
            install_puts_i18n("install2disk: invalid driver component name",
                              "install2disk: 无效 driver 组件名 (invalid driver component name)");
            return 0;
        }
        if (install_repair_copy_path(src_path, &copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    if (strcmp(kind, "system") == 0) {
        if (install_build_component_path(src_path, (u64)sizeof(src_path), "/system", name) == 0) {
            install_puts_i18n("install2disk: invalid system component name",
                              "install2disk: 无效 system 组件名 (invalid system component name)");
            return 0;
        }
        if (install_repair_copy_path(src_path, &copied_files, &copied_bytes) == 0) {
            return 0;
        }
        return install_repair_manifest();
    }

    (void)printf(INSTALL_TEXT("install2disk: unknown repair component: %s\n",
                              "install2disk: 未知 repair 组件 (unknown repair component): %s\n"),
                 kind);
    install_print_usage();
    return 0;
}

static int install_repair_interactive(void) {
    int choice;
    char name[INSTALL_REPAIR_NAME_MAX];

    install_puts_i18n("install2disk: repair components:", "install2disk: 修复组件 (repair components):");
    install_puts_i18n("  [l] full Limine BIOS chain (recommended)",
                      "  [l] 完整 Limine BIOS 链 (full Limine BIOS chain, recommended)");
    install_puts_i18n("  [b] bootloader MBR/stage2 sectors",
                      "  [b] 引导器 MBR/stage2 扇区 (bootloader MBR/stage2 sectors)");
    install_puts_i18n("  [m] install manifest", "  [m] 安装清单 (install manifest)");
    install_puts_i18n("  [c] Limine config files", "  [c] Limine 配置文件 (config files)");
    install_puts_i18n("  [s] Limine sys file", "  [s] Limine sys 文件 (sys file)");
    install_puts_i18n("  [a] /shell app ELF", "  [a] /shell 应用 ELF (app ELF)");
    install_puts_i18n("  [u] /shell/uwm app ELF", "  [u] /shell/uwm 应用 ELF (app ELF)");
    install_puts_i18n("  [d] /driver ELF", "  [d] /driver ELF");
    install_puts_i18n("  [y] /system ELF", "  [y] /system ELF");
    install_puts_i18n("  [p] absolute path", "  [p] 绝对路径 (absolute path)");
    choice = install_prompt_choice(INSTALL_TEXT("install2disk: choose repair target [l/b/m/c/s/a/u/d/y/p, q cancel]: ",
                                                "install2disk: 选择修复目标 (choose repair target) [l/b/m/c/s/a/u/d/y/p, q 取消]: "),
                                   "lbmcsaudypq", 'q');

    if (choice == 'q') {
        install_puts_i18n("install2disk: cancelled", "install2disk: 已取消 (cancelled)");
        return 0;
    }

    if (choice == 'l') {
        return install_repair_component("limine", (const char *)0);
    }
    if (choice == 'b') {
        return install_repair_component("bootloader", (const char *)0);
    }
    if (choice == 'm') {
        return install_repair_component("manifest", (const char *)0);
    }
    if (choice == 'c') {
        return install_repair_component("limine-conf", (const char *)0);
    }
    if (choice == 's') {
        return install_repair_component("limine-sys", (const char *)0);
    }

    if (choice == 'p') {
        install_read_line(INSTALL_TEXT("install2disk: absolute source path from ISO/rootfs: ",
                                       "install2disk: ISO/rootfs 中的绝对源路径 (absolute source path): "),
                          name, (u64)sizeof(name));
        return install_repair_component("path", name);
    }

    install_read_line(INSTALL_TEXT("install2disk: component name: ",
                                   "install2disk: 组件名称 (component name): "),
                      name, (u64)sizeof(name));
    if (choice == 'a') {
        return install_repair_component("shell", name);
    }
    if (choice == 'u') {
        return install_repair_component("uwm", name);
    }
    if (choice == 'd') {
        return install_repair_component("driver", name);
    }

    return install_repair_component("system", name);
}

static int install_mount_existing_disk(void) {
    if (cleonos_sys_disk_formatted() == 0ULL) {
        return 0;
    }

    if (cleonos_sys_disk_mount(INSTALL_MOUNT_PATH) == 0ULL) {
        return 0;
    }

    return 1;
}

static int install_running_from_disk_boot(void) {
    char mount_path[USH_PATH_MAX];

    mount_path[0] = '\0';
    if (cleonos_sys_disk_mounted() == 0ULL) {
        return 0;
    }

    if (cleonos_sys_disk_mount_path(mount_path, (u64)sizeof(mount_path)) == 0ULL) {
        return 0;
    }

    return (strcmp(mount_path, "/") == 0) ? 1 : 0;
}

static void install_update_note_verify_failed(install_update_result *result, const char *path) {
    u64 slot;

    if (result == (install_update_result *)0) {
        return;
    }

    slot = result->verify_failed;
    result->verify_failed++;
    if (slot >= (u64)INSTALL_UPDATE_VERIFY_SAMPLE_MAX || path == (const char *)0) {
        return;
    }

    (void)snprintf(result->verify_failed_paths[slot], (unsigned long)sizeof(result->verify_failed_paths[slot]), "%s",
                   path);
}

static void install_update_print_verify_summary(const install_update_result *result) {
    u64 i;
    u64 shown;

    if (result == (const install_update_result *)0 || result->verify_failed == 0ULL) {
        return;
    }

    (void)printf(INSTALL_TEXT("install2disk: update completed with %llu verify failure(s)\n",
                              "install2disk: 更新完成，但有 %llu 个验证失败 (verify failure)\n"),
                 (unsigned long long)result->verify_failed);

    shown = result->verify_failed;
    if (shown > (u64)INSTALL_UPDATE_VERIFY_SAMPLE_MAX) {
        shown = (u64)INSTALL_UPDATE_VERIFY_SAMPLE_MAX;
    }

    for (i = 0ULL; i < shown; i++) {
        if (result->verify_failed_paths[i][0] == '\0') {
            continue;
        }
        (void)printf(INSTALL_TEXT("install2disk: verify failed item: %s\n",
                                  "install2disk: 验证失败项目 (verify failed item): %s\n"),
                     result->verify_failed_paths[i]);
    }

    if (result->verify_failed > shown) {
        (void)printf(INSTALL_TEXT("install2disk: verify failed item: ... %llu more\n",
                                  "install2disk: 验证失败项目 (verify failed item): ... 还有 %llu 个\n"),
                     (unsigned long long)(result->verify_failed - shown));
    }
}

static int install_update_overwrite_checked(const char *src, const char *dst, const char *logical_path,
                                            install_update_result *result, u64 *copied_files, u64 *copied_bytes,
                                            install_progress *progress) {
    u64 expected_size;
    u64 actual_size;
    char expected_hash[INSTALL_SHA256_HEX_LEN + 1U];
    char actual_hash[INSTALL_SHA256_HEX_LEN + 1U];

    if (src == (const char *)0 || dst == (const char *)0 || result == (install_update_result *)0) {
        return 0;
    }

    expected_size = cleonos_sys_fs_stat_size(src);
    expected_hash[0] = '\0';
    actual_hash[0] = '\0';
    if (install_sha256_file_hex(src, expected_hash) == 0) {
        expected_hash[0] = '\0';
    }

    if (install_overwrite_file_whole(src, dst, copied_files, copied_bytes, progress) == 0) {
        actual_size = cleonos_sys_fs_stat_size(dst);
        if (expected_size != (u64)-1 && actual_size != expected_size) {
            install_update_note_verify_failed(result, (logical_path != (const char *)0) ? logical_path : dst);
            return 1;
        }
        return 0;
    }

    actual_size = cleonos_sys_fs_stat_size(dst);
    if (expected_hash[0] != '\0' && install_sha256_file_hex(dst, actual_hash) == 0) {
        actual_hash[0] = '\0';
    }

    if (expected_size == (u64)-1 || actual_size != expected_size ||
        (expected_hash[0] != '\0' && strcmp(expected_hash, actual_hash) != 0)) {
        (void)printf(INSTALL_TEXT("install2disk: update verify failed: %s\n",
                                  "install2disk: 更新验证失败 (update verify failed): %s\n"),
                     (logical_path != (const char *)0) ? logical_path : dst);
        install_update_note_verify_failed(result, (logical_path != (const char *)0) ? logical_path : dst);
    }

    return 1;
}

static int install_update_copy_file_unverified(const char *src, const char *dst, u64 *copied_files,
                                               u64 *copied_bytes, install_progress *progress) {
    static char buffer[INSTALL_COPY_CHUNK_SIZE];
    u64 size;
    u64 dst_type;
    u64 src_fd;
    u64 got;
    u64 wrote;
    u64 file_offset = 0ULL;

    size = cleonos_sys_fs_stat_size(src);
    if (size == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: stat failed: %s\n",
                                  "install2disk: 获取文件信息失败 (stat failed): %s\n"),
                     src);
        return 0;
    }

    dst_type = cleonos_sys_fs_stat_type(dst);
    if (dst_type == 2ULL) {
        (void)printf(INSTALL_TEXT("install2disk: target is directory: %s\n",
                                  "install2disk: 目标是目录 (target is directory): %s\n"),
                     dst);
        return 0;
    }
    if (dst_type == 1ULL && cleonos_sys_fs_remove(dst) == 0ULL) {
        (void)printf(INSTALL_TEXT("install2disk: remove old target failed: %s\n",
                                  "install2disk: 删除旧目标失败 (remove old target failed): %s\n"),
                     dst);
        return 0;
    }

    if (size == 0ULL) {
        if (cleonos_sys_fs_write(dst, "", 0ULL) == 0ULL) {
            (void)printf(INSTALL_TEXT("install2disk: create failed: %s\n",
                                      "install2disk: 创建失败 (create failed): %s\n"),
                         dst);
            return 0;
        }
        if (copied_files != (u64 *)0) {
            *copied_files += 1ULL;
        }
        install_progress_finish_item(progress);
        return 1;
    }

    if (size <= INSTALL_WHOLE_FILE_LIMIT) {
        char *whole = (char *)malloc((size_t)size);

        if (whole != (char *)0) {
            src_fd = cleonos_sys_fd_open(src, CLEONOS_O_RDONLY, 0ULL);
            if (src_fd == (u64)-1) {
                free(whole);
                (void)printf(INSTALL_TEXT("install2disk: open read failed: %s\n",
                                          "install2disk: 打开读取失败 (open read failed): %s\n"),
                             src);
                return 0;
            }

            while (file_offset < size) {
                u64 want = size - file_offset;

                if (want > (u64)INSTALL_COPY_CHUNK_SIZE) {
                    want = (u64)INSTALL_COPY_CHUNK_SIZE;
                }

                got = cleonos_sys_fd_read(src_fd, whole + (size_t)file_offset, want);
                if (got == (u64)-1 || got == 0ULL) {
                    (void)cleonos_sys_fd_close(src_fd);
                    free(whole);
                    (void)printf(INSTALL_TEXT("install2disk: read failed: %s\n",
                                              "install2disk: 读取失败 (read failed): %s\n"),
                                 src);
                    return 0;
                }

                file_offset += got;
            }

            (void)cleonos_sys_fd_close(src_fd);
            wrote = cleonos_sys_fs_write(dst, whole, size);
            free(whole);

            if (wrote != size) {
                (void)printf(INSTALL_TEXT("install2disk: write failed: %s\n",
                                          "install2disk: 写入失败 (write failed): %s\n"),
                             dst);
                return 0;
            }

            if (copied_bytes != (u64 *)0) {
                *copied_bytes += size;
            }
            if (copied_files != (u64 *)0) {
                *copied_files += 1ULL;
            }
            install_progress_add_done_bytes(progress, size);
            install_progress_finish_item(progress);
            return 1;
        }
    }

    src_fd = cleonos_sys_fd_open(src, CLEONOS_O_RDONLY, 0ULL);
    if (src_fd == (u64)-1) {
        (void)printf(INSTALL_TEXT("install2disk: open read failed: %s\n",
                                  "install2disk: 打开读取失败 (open read failed): %s\n"),
                     src);
        return 0;
    }

    if (cleonos_sys_fs_write(dst, "", 0ULL) == 0ULL) {
        (void)cleonos_sys_fd_close(src_fd);
        (void)printf(INSTALL_TEXT("install2disk: create failed: %s\n",
                                  "install2disk: 创建失败 (create failed): %s\n"),
                     dst);
        return 0;
    }

    for (;;) {
        got = cleonos_sys_fd_read(src_fd, buffer, (u64)sizeof(buffer));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(src_fd);
            (void)printf(INSTALL_TEXT("install2disk: read failed: %s\n",
                                      "install2disk: 读取失败 (read failed): %s\n"),
                         src);
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        wrote = cleonos_sys_fs_append(dst, buffer, got);
        if (wrote != got) {
            (void)cleonos_sys_fd_close(src_fd);
            (void)printf(INSTALL_TEXT("install2disk: write failed: %s\n",
                                      "install2disk: 写入失败 (write failed): %s\n"),
                         dst);
            return 0;
        }

        file_offset += got;
        if (copied_bytes != (u64 *)0) {
            *copied_bytes += got;
        }
        install_progress_add_done_bytes(progress, got);
    }

    (void)cleonos_sys_fd_close(src_fd);
    if (copied_files != (u64 *)0) {
        *copied_files += 1ULL;
    }
    install_progress_finish_item(progress);
    return 1;
}

static int install_update_shell_target_matches(const install_manifest_entry *entry) {
    char target_path[USH_PATH_MAX];
    char actual_hash[INSTALL_SHA256_HEX_LEN + 1U];
    u64 actual_size;

    if (entry == (const install_manifest_entry *)0) {
        return 0;
    }

    if (install_root_join(target_path, (u64)sizeof(target_path), INSTALL_MOUNT_PATH, entry->path) == 0) {
        return 0;
    }

    if (cleonos_sys_fs_stat_type(target_path) != 1ULL) {
        return 0;
    }

    actual_size = cleonos_sys_fs_stat_size(target_path);
    if (actual_size != entry->size) {
        return 0;
    }

    if (install_sha256_file_hex(target_path, actual_hash) == 0) {
        return 0;
    }

    return (strcmp(actual_hash, entry->hash) == 0) ? 1 : 0;
}

static int install_update_shell_plan_copy(const install_manifest_entry *entry,
                                          const install_manifest_entry *old_entry) {
    if (entry == (const install_manifest_entry *)0) {
        return 0;
    }

    if (old_entry == (const install_manifest_entry *)0) {
        return 1;
    }

    if (install_manifest_entry_same(entry, old_entry) == 0) {
        return 1;
    }

    return (install_update_shell_target_matches(entry) == 0) ? 1 : 0;
}

static int install_update_shell_copy_entry(const install_manifest_entry *entry, install_update_result *result,
                                           install_progress *progress) {
    char source_path[USH_PATH_MAX];
    char target_path[USH_PATH_MAX];

    if (entry == (const install_manifest_entry *)0 || result == (install_update_result *)0) {
        return 0;
    }

    if (install_root_join(source_path, (u64)sizeof(source_path), "/", entry->path) == 0 ||
        install_root_join(target_path, (u64)sizeof(target_path), INSTALL_MOUNT_PATH, entry->path) == 0) {
        (void)printf(INSTALL_TEXT("install2disk: update path too long: %s\n",
                                  "install2disk: 更新路径过长 (update path too long): %s\n"),
                     entry->path);
        return 0;
    }

    if (install_mkdir_parents_for_file(target_path) == 0) {
        (void)printf(INSTALL_TEXT("install2disk: update mkdir parents failed: %s\n",
                                  "install2disk: 更新时创建父目录失败 (update mkdir parents failed): %s\n"),
                     target_path);
        return 0;
    }

    if (install_update_copy_file_unverified(source_path, target_path, &result->copied_files, &result->copied_bytes,
                                            progress) == 0) {
        return 0;
    }

    if (install_update_shell_target_matches(entry) == 0) {
        (void)printf(INSTALL_TEXT("install2disk: update verify failed: %s\n",
                                  "install2disk: 更新验证失败 (update verify failed): %s\n"),
                     entry->path);
        install_update_note_verify_failed(result, entry->path);
        return 1;
    }

    return 1;
}

static int install_update_shell_delete_obsolete(install_manifest_entry *old_entries, u64 old_count,
                                                install_manifest_entry *new_entries, u64 new_count, int dry_run,
                                                install_update_result *result) {
    u64 i;

    if (old_entries == (install_manifest_entry *)0 || new_entries == (install_manifest_entry *)0 ||
        result == (install_update_result *)0) {
        return 0;
    }

    for (i = 0ULL; i < old_count; i++) {
        char target_path[USH_PATH_MAX];
        u64 type;

        if (install_update_shell_path_included(old_entries[i].path) == 0) {
            continue;
        }

        if (install_manifest_find(new_entries, new_count, old_entries[i].path) != (install_manifest_entry *)0) {
            continue;
        }

        if (install_root_join(target_path, (u64)sizeof(target_path), INSTALL_MOUNT_PATH, old_entries[i].path) == 0) {
            (void)printf(INSTALL_TEXT("install2disk: obsolete path too long: %s\n",
                                      "install2disk: 过期文件路径过长 (obsolete path too long): %s\n"),
                         old_entries[i].path);
            return 0;
        }

        type = cleonos_sys_fs_stat_type(target_path);
        if (type == (u64)-1) {
            continue;
        }

        if (type != 1ULL) {
            (void)printf(INSTALL_TEXT("install2disk: obsolete path is not a file, skipped: %s\n",
                                      "install2disk: 过期路径不是文件，已跳过 (not a file, skipped): %s\n"),
                         old_entries[i].path);
            continue;
        }

        if (dry_run == 0 && cleonos_sys_fs_remove(target_path) == 0ULL) {
            (void)printf(INSTALL_TEXT("install2disk: remove obsolete file failed: %s\n",
                                      "install2disk: 删除过期文件失败 (remove obsolete file failed): %s\n"),
                         target_path);
            return 0;
        }

        result->deleted++;
        (void)printf(INSTALL_TEXT("install2disk: obsolete %s%s\n", "install2disk: 过期文件 (obsolete) %s%s\n"),
                     dry_run != 0 ? INSTALL_TEXT("would remove: ", "将删除 (would remove): ")
                                  : INSTALL_TEXT("removed: ", "已删除 (removed): "),
                     old_entries[i].path);
    }

    return 1;
}

static int install_update_shell_write_state_files(void) {
    static const char state_text[] = "shell-update\n";
    u64 copied_files = 0ULL;
    u64 copied_bytes = 0ULL;

    if (install_mkdir(INSTALL_MOUNT_PATH "/system") == 0) {
        return 0;
    }

    if (cleonos_sys_fs_write(INSTALL_UPDATE_STATE_TARGET, state_text, (u64)strlen(state_text)) !=
        (u64)strlen(state_text)) {
        install_puts_i18n("install2disk: write update state failed",
                          "install2disk: 写入更新状态失败 (write update state failed)");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(INSTALL_MANIFEST_TARGET) == 1ULL &&
        install_copy_file(INSTALL_MANIFEST_TARGET, INSTALL_MANIFEST_PREV_TARGET, &copied_files, &copied_bytes,
                          (install_progress *)0) == 0) {
        return 0;
    }

    if (install_copy_file(INSTALL_DRYRUN_MANIFEST, INSTALL_MANIFEST_NEW_TARGET, &copied_files, &copied_bytes,
                          (install_progress *)0) == 0) {
        return 0;
    }

    return 1;
}

static int install_update_shell_refresh_manifest(install_manifest_entry *old_entries, u64 old_count,
                                                 install_manifest_entry *new_entries, u64 new_count) {
    install_manifest_writer writer;
    u64 i;

    if (old_entries == (install_manifest_entry *)0 || new_entries == (install_manifest_entry *)0) {
        return 0;
    }

    if (cleonos_sys_fs_stat_type(INSTALL_MANIFEST_TARGET) == 1ULL &&
        cleonos_sys_fs_remove(INSTALL_MANIFEST_TARGET) == 0ULL) {
        install_puts_i18n("install2disk: remove old manifest failed",
                          "install2disk: 删除旧 manifest 失败 (remove old manifest failed)");
        return 0;
    }

    if (cleonos_sys_fs_write(INSTALL_MANIFEST_TARGET, "", 0ULL) == 0ULL) {
        install_puts_i18n("install2disk: create manifest failed",
                          "install2disk: 创建 manifest 失败 (create manifest failed)");
        return 0;
    }

    memset(&writer, 0, sizeof(writer));

    for (i = 0ULL; i < old_count; i++) {
        char target_path[USH_PATH_MAX];

        if (install_update_shell_path_included(old_entries[i].path) != 0 ||
            install_manifest_path_included(old_entries[i].path) == 0) {
            continue;
        }

        if (install_root_join(target_path, (u64)sizeof(target_path), INSTALL_MOUNT_PATH, old_entries[i].path) == 0) {
            (void)printf(INSTALL_TEXT("install2disk: manifest path too long: %s\n",
                                      "install2disk: manifest 路径过长 (path too long): %s\n"),
                         old_entries[i].path);
            return 0;
        }

        if (cleonos_sys_fs_stat_type(target_path) != 1ULL) {
            continue;
        }

        if (install_manifest_append_entry(INSTALL_MANIFEST_TARGET, target_path, old_entries[i].path, &writer) == 0) {
            return 0;
        }
    }

    for (i = 0ULL; i < new_count; i++) {
        char target_path[USH_PATH_MAX];

        if (install_root_join(target_path, (u64)sizeof(target_path), INSTALL_MOUNT_PATH, new_entries[i].path) == 0) {
            (void)printf(INSTALL_TEXT("install2disk: manifest path too long: %s\n",
                                      "install2disk: manifest 路径过长 (path too long): %s\n"),
                         new_entries[i].path);
            return 0;
        }

        if (install_manifest_append_entry(INSTALL_MANIFEST_TARGET, target_path, new_entries[i].path, &writer) == 0) {
            return 0;
        }
    }

    if (writer.files == 0ULL) {
        install_puts_i18n("install2disk: refreshed manifest has no files",
                          "install2disk: 刷新后的 manifest 没有文件 (has no files)");
        return 0;
    }

    (void)printf(INSTALL_TEXT("install2disk: manifest refreshed: %llu files, %llu bytes, %llu manifest bytes\n",
                              "install2disk: manifest 已刷新 (refreshed): %llu 文件 (files), %llu 字节 (bytes), %llu manifest 字节 (manifest bytes)\n"),
                 (unsigned long long)writer.files, (unsigned long long)writer.file_bytes,
                 (unsigned long long)writer.manifest_bytes);
    return 1;
}

static void install_update_shell_cleanup_state(void) {
    if (cleonos_sys_fs_stat_type(INSTALL_UPDATE_STATE_TARGET) == 1ULL) {
        (void)cleonos_sys_fs_remove(INSTALL_UPDATE_STATE_TARGET);
    }
    if (cleonos_sys_fs_stat_type(INSTALL_MANIFEST_NEW_TARGET) == 1ULL) {
        (void)cleonos_sys_fs_remove(INSTALL_MANIFEST_NEW_TARGET);
    }
    if (cleonos_sys_fs_stat_type(INSTALL_DRYRUN_MANIFEST) == 1ULL) {
        (void)cleonos_sys_fs_remove(INSTALL_DRYRUN_MANIFEST);
    }
}

static int install_update_shell(int dry_run) {
    install_update_result result;
    install_progress progress;
    u64 old_count;
    u64 new_count;
    u64 i;

    install_stage((dry_run != 0) ? "plan shell update" : "update shell");
    install_update_last_failure_was_verify = 0;

    if (cleonos_sys_disk_present() == 0ULL) {
        install_puts_i18n("install2disk: disk not present", "install2disk: 未检测到硬盘 (disk not present)");
        return 0;
    }

    if (install_running_from_disk_boot() != 0) {
        install_puts_i18n("install2disk: refused: current system is booted from disk",
                          "install2disk: 已拒绝：当前系统从硬盘启动 (booted from disk)");
        install_puts_i18n("install2disk: boot the ISO installer before updating the disk shell",
                          "install2disk: 更新硬盘外壳前请先启动 ISO 安装器 (boot ISO installer)");
        return 0;
    }

    if (install_mount_existing_disk() == 0) {
        install_puts_i18n("install2disk: mount existing disk failed",
                          "install2disk: 挂载已有硬盘失败 (mount existing disk failed)");
        return 0;
    }

    if (install_generate_manifest_to("/", INSTALL_DRYRUN_MANIFEST, install_update_shell_path_included) == 0) {
        return 0;
    }

    old_count = install_manifest_load(INSTALL_MANIFEST_TARGET, install_update_old_entries,
                                      (u64)INSTALL_UPDATE_MAX_ENTRIES, 1);
    if (old_count == (u64)-1) {
        return 0;
    }

    new_count = install_manifest_load(INSTALL_DRYRUN_MANIFEST, install_update_new_entries,
                                      (u64)INSTALL_UPDATE_MAX_ENTRIES, 0);
    if (new_count == (u64)-1 || new_count == 0ULL) {
        install_puts_i18n("install2disk: no shell update entries found",
                          "install2disk: 未找到外壳更新条目 (no shell update entries found)");
        return 0;
    }

    memset(&result, 0, sizeof(result));
    memset(&progress, 0, sizeof(progress));
    progress.label = (dry_run != 0) ? "plan shell files" : "update shell files";

    for (i = 0ULL; i < new_count; i++) {
        install_manifest_entry *old_entry = install_manifest_find(install_update_old_entries, old_count,
                                                                  install_update_new_entries[i].path);
        if (install_update_shell_plan_copy(install_update_new_entries + i, old_entry) == 0) {
            result.unchanged++;
            continue;
        }

        if (old_entry == (install_manifest_entry *)0) {
            result.added++;
        } else {
            result.updated++;
        }

        {
            char source_path[USH_PATH_MAX];
            if (install_root_join(source_path, (u64)sizeof(source_path), "/", install_update_new_entries[i].path) ==
                0) {
                (void)printf(INSTALL_TEXT("install2disk: update source path too long: %s\n",
                                          "install2disk: 更新源路径过长 (update source path too long): %s\n"),
                             install_update_new_entries[i].path);
                return 0;
            }
            install_progress_plan_file(&progress, source_path, 1ULL);
        }
    }

    if (install_update_shell_delete_obsolete(install_update_old_entries, old_count, install_update_new_entries,
                                             new_count, 1, &result) == 0) {
        return 0;
    }

    (void)printf(INSTALL_TEXT("install2disk: shell update plan: added=%llu updated=%llu unchanged=%llu removed=%llu\n",
                              "install2disk: 外壳更新计划 (shell update plan): 新增 (added)=%llu 更新 (updated)=%llu 不变 (unchanged)=%llu 删除 (removed)=%llu\n"),
                 (unsigned long long)result.added, (unsigned long long)result.updated,
                 (unsigned long long)result.unchanged, (unsigned long long)result.deleted);

    if (dry_run != 0) {
        return 1;
    }

    if (install_update_shell_write_state_files() == 0) {
        return 0;
    }

    install_progress_print(&progress, 1);
    for (i = 0ULL; i < new_count; i++) {
        install_manifest_entry *old_entry = install_manifest_find(install_update_old_entries, old_count,
                                                                  install_update_new_entries[i].path);
        if (install_update_shell_plan_copy(install_update_new_entries + i, old_entry) == 0) {
            continue;
        }

        if (install_update_shell_copy_entry(install_update_new_entries + i, &result, &progress) == 0) {
            return 0;
        }
    }
    progress.done_items = progress.total_items;
    progress.done_bytes = progress.total_bytes;
    install_progress_print(&progress, 1);

    result.deleted = 0ULL;
    if (install_update_shell_delete_obsolete(install_update_old_entries, old_count, install_update_new_entries,
                                             new_count, 0, &result) == 0) {
        return 0;
    }

    if (result.verify_failed != 0ULL) {
        install_update_shell_cleanup_state();
        install_update_print_verify_summary(&result);
        install_puts_i18n("install2disk: manifest not refreshed because some updated files failed verification",
                          "install2disk: 部分更新文件验证失败，未刷新 manifest (manifest not refreshed)");
        install_update_last_failure_was_verify = 1;
        return 0;
    }

    install_stage("refresh install manifest");
    if (install_update_shell_refresh_manifest(install_update_old_entries, old_count, install_update_new_entries,
                                              new_count) == 0) {
        return 0;
    }

    install_update_shell_cleanup_state();
    (void)printf(INSTALL_TEXT("install2disk: shell updated: copied=%llu files %llu bytes, removed=%llu\n",
                              "install2disk: 外壳已更新 (shell updated): 已复制 (copied)=%llu 文件 (files) %llu 字节 (bytes), 已删除 (removed)=%llu\n"),
                 (unsigned long long)result.copied_files, (unsigned long long)result.copied_bytes,
                 (unsigned long long)result.deleted);
    install_puts_i18n("install2disk: preserved /home, /system/users.db, /system/pkg and untracked files",
                      "install2disk: 已保留 /home、/system/users.db、/system/pkg 和未跟踪文件 (preserved)");
    return 1;
}

static int install_update_all(void) {
    int ok = 1;

    install_stage("update kernel and shell");

    install_update_last_failure_was_verify = 0;
    if (install_update_kernel() == 0) {
        if (install_update_last_failure_was_verify == 0) {
            return 0;
        }
        ok = 0;
    }

    install_update_last_failure_was_verify = 0;
    if (install_update_shell(0) == 0) {
        if (install_update_last_failure_was_verify == 0) {
            return 0;
        }
        ok = 0;
    }

    return ok;
}

static int install_update_kernel(void) {
    u64 copied_files = 0ULL;
    u64 copied_bytes = 0ULL;
    install_progress progress;
    install_update_result result;

    install_stage("update kernel");
    install_update_last_failure_was_verify = 0;

    if (cleonos_sys_disk_present() == 0ULL) {
        install_puts_i18n("install2disk: disk not present", "install2disk: 未检测到硬盘 (disk not present)");
        return 0;
    }

    if (install_running_from_disk_boot() != 0) {
        install_puts_i18n("install2disk: refused: current system is booted from disk",
                          "install2disk: 已拒绝：当前系统从硬盘启动 (booted from disk)");
        install_puts_i18n("install2disk: boot the ISO installer before updating the disk kernel",
                          "install2disk: 更新硬盘内核前请先启动 ISO 安装器 (boot ISO installer)");
        return 0;
    }

    if (install_mount_existing_disk() == 0) {
        install_puts_i18n("install2disk: mount existing disk failed",
                          "install2disk: 挂载已有硬盘失败 (mount existing disk failed)");
        return 0;
    }

    memset(&progress, 0, sizeof(progress));
    memset(&result, 0, sizeof(result));
    progress.label = "update kernel";
    install_progress_plan_file(&progress, INSTALL_KERNEL_SOURCE, 1ULL);
    install_progress_plan_file(&progress, INSTALL_LIMINE_CONF_SOURCE, 4ULL);
    install_progress_print(&progress, 1);

    if (install_update_overwrite_checked(INSTALL_KERNEL_SOURCE, INSTALL_KERNEL_TARGET, "/kernel.elf", &result,
                                         &copied_files, &copied_bytes, &progress) == 0) {
        return 0;
    }

    if (install_prepare_limine_dirs() == 0 ||
        install_update_overwrite_checked(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/boot/limine/limine.conf",
                                         "/boot/limine/limine.conf", &result, &copied_files, &copied_bytes,
                                         &progress) == 0 ||
        install_update_overwrite_checked(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/boot/limine.conf",
                                         "/boot/limine.conf", &result, &copied_files, &copied_bytes, &progress) ==
            0 ||
        install_update_overwrite_checked(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/limine/limine.conf",
                                         "/limine/limine.conf", &result, &copied_files, &copied_bytes, &progress) ==
            0 ||
        install_update_overwrite_checked(INSTALL_LIMINE_CONF_SOURCE, INSTALL_MOUNT_PATH "/limine.conf",
                                         "/limine.conf", &result, &copied_files, &copied_bytes, &progress) == 0) {
        return 0;
    }

    progress.done_items = progress.total_items;
    progress.done_bytes = progress.total_bytes;
    install_progress_print(&progress, 1);

    (void)printf(INSTALL_TEXT("install2disk: kernel updated: %llu files, %llu bytes\n",
                              "install2disk: 内核已更新 (kernel updated): %llu 文件 (files), %llu 字节 (bytes)\n"),
                 (unsigned long long)copied_files, (unsigned long long)copied_bytes);
    if (result.verify_failed != 0ULL) {
        install_update_print_verify_summary(&result);
        install_update_last_failure_was_verify = 1;
        return 0;
    }
    install_puts_i18n("install2disk: install manifest not regenerated for kernel-only update",
                      "install2disk: 仅更新内核不会重新生成安装清单 (manifest not regenerated)");
    install_puts_i18n("install2disk: done. Reboot or use make run-hardboot.",
                      "install2disk: 完成。重启或使用 make run-hardboot (done)");
    return 1;
}

static int install2disk_run(void) {
    u64 copied_files = 0ULL;
    u64 copied_bytes = 0ULL;
    install_progress root_progress;

    memset(&root_progress, 0, sizeof(root_progress));
    root_progress.label = "copy root filesystem";

    if (cleonos_sys_disk_present() == 0ULL) {
        install_puts_i18n("install2disk: disk not present", "install2disk: 未检测到硬盘 (disk not present)");
        return 0;
    }

    if (install_running_from_disk_boot() != 0) {
        install_puts_i18n("install2disk: refused: current system is booted from disk",
                          "install2disk: 已拒绝：当前系统从硬盘启动 (booted from disk)");
        install_puts_i18n("install2disk: boot the ISO installer before updating or reinstalling this disk",
                          "install2disk: 更新或重装硬盘前请先启动 ISO 安装器 (boot ISO installer)");
        return 0;
    }

    {
        int choice;

        choice = install_prompt_choice(
            INSTALL_TEXT("install2disk: choose [a]update kernel+shell, [u]kernel only, [s]shell only, [r]epair, [f]ormat full install, [c]ancel (default: a): ",
                         "install2disk: 选择 (choose) [a]更新内核+外壳, [u]仅内核, [s]仅外壳, [r]修复, [f]格式化完整安装, [c]取消 (默认 a): "),
            "ausrfc", 'a');

        if (choice == 'c') {
            install_puts_i18n("install2disk: cancelled", "install2disk: 已取消 (cancelled)");
            return 0;
        }

        if (choice == 'a') {
            return install_update_all();
        }

        if (choice == 'u') {
            return install_update_kernel();
        }

        if (choice == 's') {
            return install_update_shell(0);
        }

        if (choice == 'r') {
            return install_repair_interactive();
        }
    }

    install_stage("scan root filesystem");
    if (install_scan_tree("/", 0ULL, &root_progress) == 0) {
        return 0;
    }
    (void)printf(INSTALL_TEXT("install2disk: rootfs plan %llu items, %llu bytes\n",
                              "install2disk: rootfs 计划 (plan) %llu 项 (items), %llu 字节 (bytes)\n"),
                 (unsigned long long)root_progress.total_items, (unsigned long long)root_progress.total_bytes);

    install_stage("format FAT32");
    if (cleonos_sys_disk_format_fat32("CLEONOS") == 0ULL) {
        install_puts_i18n("install2disk: format failed", "install2disk: 格式化失败 (format failed)");
        return 0;
    }

    install_stage("mount target disk");
    if (cleonos_sys_disk_mount(INSTALL_MOUNT_PATH) == 0ULL) {
        install_puts_i18n("install2disk: mount failed", "install2disk: 挂载失败 (mount failed)");
        return 0;
    }

    if (install_mkdir(INSTALL_MOUNT_PATH) == 0) {
        return 0;
    }

    install_stage("copy root filesystem");
    install_progress_print(&root_progress, 1);
    if (install_copy_tree("/", INSTALL_MOUNT_PATH, 0ULL, &copied_files, &copied_bytes, &root_progress) == 0) {
        return 0;
    }
    root_progress.done_bytes = root_progress.total_bytes;
    root_progress.done_items = root_progress.total_items;
    install_progress_print(&root_progress, 1);

    install_stage("install kernel and Limine files");
    if (install_prepare_boot_files(&copied_files, &copied_bytes, (install_progress *)0) == 0) {
        return 0;
    }

    if (install_setup_root_account() == 0) {
        return 0;
    }

    install_stage("write Limine BIOS boot stages");
    if (install_limine_bios((install_progress *)0) == 0) {
        return 0;
    }

    install_stage("generate install manifest");
    if (install_generate_manifest(INSTALL_MOUNT_PATH) == 0) {
        return 0;
    }

    (void)printf(INSTALL_TEXT("install2disk: installed %llu files, %llu bytes\n",
                              "install2disk: 已安装 (installed) %llu 文件 (files), %llu 字节 (bytes)\n"),
                 (unsigned long long)copied_files,
                 (unsigned long long)copied_bytes);
    install_puts_i18n("install2disk: done. Use make run-hardboot to boot from disk.",
                      "install2disk: 完成。使用 make run-hardboot 从硬盘启动 (done)");
    return 1;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc > 1 && argv != (char **)0 && argv[1] != (char *)0) {
        if (strcmp(argv[1], "update") == 0 &&
            (argc <= 2 || argv[2] == (char *)0 || strcmp(argv[2], "all") == 0)) {
            return (install_update_all() != 0) ? 0 : 1;
        }

        if (strcmp(argv[1], "update-kernel") == 0 || strcmp(argv[1], "kernel") == 0 ||
            (strcmp(argv[1], "update") == 0 && argc > 2 && argv[2] != (char *)0 &&
             strcmp(argv[2], "kernel") == 0)) {
            return (install_update_kernel() != 0) ? 0 : 1;
        }

        if (strcmp(argv[1], "update-shell") == 0 || strcmp(argv[1], "shell") == 0 ||
            (strcmp(argv[1], "update") == 0 && argc > 2 && argv[2] != (char *)0 &&
             strcmp(argv[2], "shell") == 0)) {
            int dry_run = 0;

            if ((argc > 2 && argv[2] != (char *)0 && strcmp(argv[2], "--dry-run") == 0) ||
                (argc > 3 && argv[3] != (char *)0 && strcmp(argv[3], "--dry-run") == 0)) {
                dry_run = 1;
            }

            return (install_update_shell(dry_run) != 0) ? 0 : 1;
        }

        if (strcmp(argv[1], "repair") == 0) {
            const char *kind = (argc > 2 && argv[2] != (char *)0) ? argv[2] : (const char *)0;
            const char *name = (argc > 3 && argv[3] != (char *)0) ? argv[3] : (const char *)0;

            if (kind == (const char *)0) {
                return (install_repair_interactive() != 0) ? 0 : 1;
            }

            return (install_repair_component(kind, name) != 0) ? 0 : 1;
        }

        if (strcmp(argv[1], "verify") == 0 || strcmp(argv[1], "check") == 0) {
            return (install_verify_run() != 0) ? 0 : 1;
        }

        if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            install_print_usage();
            return 0;
        }

        install_print_usage();
        return 1;
    }

    return (install2disk_run() != 0) ? 0 : 1;
}
