#include "cmd_runtime.h"
#include <stdio.h>
#include <stdlib.h>

#define PARTCTL_SECTOR_SIZE 512U
#define PARTCTL_MBR_ENTRY_OFFSET 446U
#define PARTCTL_MBR_ENTRY_SIZE 16U
#define PARTCTL_MBR_ENTRY_COUNT 4U
#define PARTCTL_MBR_SIG_OFFSET 510U

static unsigned int partctl_read_u32_le(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U) | ((unsigned int)ptr[2] << 16U) |
           ((unsigned int)ptr[3] << 24U);
}

static void partctl_write_u32_le(unsigned char *ptr, unsigned int value) {
    ptr[0] = (unsigned char)(value & 0xFFU);
    ptr[1] = (unsigned char)((value >> 8U) & 0xFFU);
    ptr[2] = (unsigned char)((value >> 16U) & 0xFFU);
    ptr[3] = (unsigned char)((value >> 24U) & 0xFFU);
}

static int partctl_parse_u64(const char *text, u64 *out_value) {
    char *end = (char *)0;
    unsigned long long value;

    if (text == (const char *)0 || text[0] == '\0' || out_value == (u64 *)0) {
        return 0;
    }

    value = strtoull(text, &end, 0);
    if (end == text || *end != '\0') {
        return 0;
    }

    *out_value = (u64)value;
    return 1;
}

static int partctl_tokenize(const char *arg, char tokens[][USH_ARG_MAX], int max_tokens) {
    int count = 0;
    u64 i = 0ULL;

    if (arg == (const char *)0 || max_tokens <= 0) {
        return 0;
    }

    while (arg[i] != '\0' && count < max_tokens) {
        u64 tlen = 0ULL;

        while (arg[i] != '\0' && ush_is_space(arg[i]) != 0) {
            i++;
        }

        if (arg[i] == '\0') {
            break;
        }

        while (arg[i] != '\0' && ush_is_space(arg[i]) == 0) {
            if (tlen + 1ULL < USH_ARG_MAX) {
                tokens[count][tlen++] = arg[i];
            }
            i++;
        }

        tokens[count][tlen] = '\0';
        count++;
    }

    return count;
}

static int partctl_disk_required(void) {
    if (cleonos_sys_disk_present() == 0ULL) {
        ush_writeln_i18n("partctl: disk not present", "partctl: 磁盘不存在");
        return 0;
    }
    return 1;
}

static int partctl_read_sector0(unsigned char *sector) {
    if (sector == (unsigned char *)0) {
        return 0;
    }

    if (partctl_disk_required() == 0) {
        return 0;
    }

    if (cleonos_sys_disk_read_sector(0ULL, (void *)sector) == 0ULL) {
        ush_writeln_i18n("partctl: read sector0 failed", "partctl: 读取 sector0 失败");
        return 0;
    }

    return 1;
}

static int partctl_write_sector0(const unsigned char *sector) {
    if (sector == (const unsigned char *)0) {
        return 0;
    }

    if (partctl_disk_required() == 0) {
        return 0;
    }

    if (cleonos_sys_disk_write_sector(0ULL, (const void *)sector) == 0ULL) {
        ush_writeln_i18n("partctl: write sector0 failed", "partctl: 写入 sector0 失败");
        return 0;
    }

    return 1;
}

static int partctl_mbr_has_signature(const unsigned char *sector) {
    return (sector[PARTCTL_MBR_SIG_OFFSET] == 0x55U && sector[PARTCTL_MBR_SIG_OFFSET + 1U] == 0xAAU) ? 1 : 0;
}

static void partctl_mbr_ensure_signature(unsigned char *sector) {
    sector[PARTCTL_MBR_SIG_OFFSET] = 0x55U;
    sector[PARTCTL_MBR_SIG_OFFSET + 1U] = 0xAAU;
}

static void partctl_usage(void) {
    ush_writeln_i18n("partctl usage:", "partctl 用法:");
    ush_writeln("  partctl list");
    ush_writeln("  partctl init-mbr");
    ush_writeln("  partctl create <index:1-4> <start_lba> <sectors> <type_hex> [boot:0|1]");
    ush_writeln("  partctl delete <index:1-4>");
    ush_writeln("  partctl set-boot <index:1-4> <0|1>");
}

static int partctl_cmd_list(void) {
    unsigned char sector[PARTCTL_SECTOR_SIZE];
    u64 disk_sectors;
    u64 i;
    int has_any = 0;

    if (partctl_read_sector0(sector) == 0) {
        return 0;
    }

    disk_sectors = cleonos_sys_disk_sector_count();
    (void)printf("disk.sectors: %llu\n", (unsigned long long)disk_sectors);
    (void)printf("mbr.signature: %s\n", partctl_mbr_has_signature(sector) != 0 ? "0x55AA" : "invalid");
    (void)fputs("idx  boot  type  start_lba    sectors      end_lba\n", 1);

    for (i = 0ULL; i < PARTCTL_MBR_ENTRY_COUNT; i++) {
        u64 off = PARTCTL_MBR_ENTRY_OFFSET + (i * PARTCTL_MBR_ENTRY_SIZE);
        unsigned char boot = sector[off + 0ULL];
        unsigned char type = sector[off + 4ULL];
        unsigned int start = partctl_read_u32_le(&sector[off + 8ULL]);
        unsigned int size = partctl_read_u32_le(&sector[off + 12ULL]);
        u64 end_lba = 0ULL;

        if (size != 0U) {
            has_any = 1;
            end_lba = (u64)start + (u64)size - 1ULL;
        }

        (void)printf("%llu    %s   0x%02X  %-12llu %-12llu ", (unsigned long long)(i + 1ULL),
                     (boot == 0x80U) ? "yes " : "no  ", (unsigned int)type, (unsigned long long)start,
                     (unsigned long long)size);

        if (size == 0U) {
            (void)fputs("-\n", 1);
        } else {
            (void)printf("%llu\n", (unsigned long long)end_lba);
        }
    }

    if (has_any == 0) {
        ush_writeln_i18n("partctl: no partition entries", "partctl: 没有分区项");
    }

    return 1;
}

static int partctl_check_overlap(const unsigned char *sector, u64 skip_index, u64 start, u64 sectors) {
    u64 i;
    u64 end = start + sectors;

    for (i = 0ULL; i < PARTCTL_MBR_ENTRY_COUNT; i++) {
        u64 off;
        unsigned int ex_start;
        unsigned int ex_size;
        u64 ex_end;

        if (i == skip_index) {
            continue;
        }

        off = PARTCTL_MBR_ENTRY_OFFSET + (i * PARTCTL_MBR_ENTRY_SIZE);
        ex_start = partctl_read_u32_le(&sector[off + 8ULL]);
        ex_size = partctl_read_u32_le(&sector[off + 12ULL]);

        if (ex_size == 0U) {
            continue;
        }

        ex_end = (u64)ex_start + (u64)ex_size;
        if (!(end <= (u64)ex_start || start >= ex_end)) {
            return 0;
        }
    }

    return 1;
}

static int partctl_cmd_init_mbr(void) {
    unsigned char sector[PARTCTL_SECTOR_SIZE];

    if (partctl_disk_required() == 0) {
        return 0;
    }

    ush_zero((void *)sector, (u64)sizeof(sector));
    partctl_mbr_ensure_signature(sector);

    if (partctl_write_sector0(sector) == 0) {
        return 0;
    }

    ush_writeln_i18n("partctl: MBR initialized", "partctl: MBR 已初始化");
    return 1;
}

static int partctl_cmd_delete(const char *index_text) {
    unsigned char sector[PARTCTL_SECTOR_SIZE];
    u64 index = 0ULL;
    u64 off;

    if (partctl_parse_u64(index_text, &index) == 0 || index < 1ULL || index > PARTCTL_MBR_ENTRY_COUNT) {
        ush_writeln_i18n("partctl: delete usage: partctl delete <index:1-4>",
                         "partctl: delete 用法: partctl delete <index:1-4>");
        return 0;
    }

    if (partctl_read_sector0(sector) == 0) {
        return 0;
    }

    off = PARTCTL_MBR_ENTRY_OFFSET + ((index - 1ULL) * PARTCTL_MBR_ENTRY_SIZE);
    ush_zero((void *)&sector[off], PARTCTL_MBR_ENTRY_SIZE);
    partctl_mbr_ensure_signature(sector);

    if (partctl_write_sector0(sector) == 0) {
        return 0;
    }

    (void)printf((ush_locale_is_zh() != 0) ? "partctl: 已删除分区 (deleted partition) %llu\n"
                                           : "partctl: deleted partition %llu\n",
                 (unsigned long long)index);
    return 1;
}

static int partctl_cmd_set_boot(const char *index_text, const char *value_text) {
    unsigned char sector[PARTCTL_SECTOR_SIZE];
    u64 index = 0ULL;
    u64 value = 0ULL;
    u64 off;
    u64 i;

    if (partctl_parse_u64(index_text, &index) == 0 || index < 1ULL || index > PARTCTL_MBR_ENTRY_COUNT) {
        ush_writeln_i18n("partctl: set-boot usage: partctl set-boot <index:1-4> <0|1>",
                         "partctl: set-boot 用法: partctl set-boot <index:1-4> <0|1>");
        return 0;
    }

    if (partctl_parse_u64(value_text, &value) == 0 || (value != 0ULL && value != 1ULL)) {
        ush_writeln_i18n("partctl: set-boot usage: partctl set-boot <index:1-4> <0|1>",
                         "partctl: set-boot 用法: partctl set-boot <index:1-4> <0|1>");
        return 0;
    }

    if (partctl_read_sector0(sector) == 0) {
        return 0;
    }

    for (i = 0ULL; i < PARTCTL_MBR_ENTRY_COUNT; i++) {
        u64 eoff = PARTCTL_MBR_ENTRY_OFFSET + (i * PARTCTL_MBR_ENTRY_SIZE);
        if (value != 0ULL && (i + 1ULL) != index) {
            sector[eoff] = 0x00U;
        }
    }

    off = PARTCTL_MBR_ENTRY_OFFSET + ((index - 1ULL) * PARTCTL_MBR_ENTRY_SIZE);
    sector[off] = (value != 0ULL) ? 0x80U : 0x00U;
    partctl_mbr_ensure_signature(sector);

    if (partctl_write_sector0(sector) == 0) {
        return 0;
    }

    (void)printf((ush_locale_is_zh() != 0) ? "partctl: 分区 (partition) %llu boot flag=%llu\n"
                                           : "partctl: partition %llu boot flag=%llu\n",
                 (unsigned long long)index, (unsigned long long)value);
    return 1;
}

static int partctl_cmd_create(const char *index_text, const char *start_text, const char *sectors_text,
                              const char *type_text, const char *boot_text) {
    unsigned char sector[PARTCTL_SECTOR_SIZE];
    u64 index = 0ULL;
    u64 start_lba = 0ULL;
    u64 sectors = 0ULL;
    u64 type = 0ULL;
    u64 boot = 0ULL;
    u64 disk_sectors;
    u64 off;
    u64 i;

    if (partctl_parse_u64(index_text, &index) == 0 || index < 1ULL || index > PARTCTL_MBR_ENTRY_COUNT) {
        ush_writeln_i18n("partctl: create invalid index", "partctl: create 的索引无效");
        return 0;
    }

    if (partctl_parse_u64(start_text, &start_lba) == 0 || partctl_parse_u64(sectors_text, &sectors) == 0 ||
        partctl_parse_u64(type_text, &type) == 0) {
        ush_writeln_i18n("partctl: create invalid number", "partctl: create 的数字无效");
        return 0;
    }

    if (boot_text != (const char *)0 && boot_text[0] != '\0') {
        if (partctl_parse_u64(boot_text, &boot) == 0 || (boot != 0ULL && boot != 1ULL)) {
            ush_writeln_i18n("partctl: create invalid boot flag (expected 0|1)",
                             "partctl: create 的 boot 标志无效（需要 0|1）");
            return 0;
        }
    }

    if (start_lba == 0ULL || sectors == 0ULL) {
        ush_writeln_i18n("partctl: start_lba and sectors must be > 0",
                         "partctl: start_lba 和 sectors 必须大于 0");
        return 0;
    }

    if (type > 0xFFULL || start_lba > 0xFFFFFFFFULL || sectors > 0xFFFFFFFFULL) {
        ush_writeln_i18n("partctl: MBR supports only 32-bit LBA/size and 8-bit type",
                         "partctl: MBR 只支持 32-bit LBA/大小和 8-bit 类型");
        return 0;
    }

    disk_sectors = cleonos_sys_disk_sector_count();
    if (start_lba >= disk_sectors || sectors > disk_sectors || (start_lba + sectors) > disk_sectors ||
        (start_lba + sectors) < start_lba) {
        ush_writeln_i18n("partctl: range exceeds disk size", "partctl: 范围超过磁盘大小");
        return 0;
    }

    if (partctl_read_sector0(sector) == 0) {
        return 0;
    }

    if (partctl_check_overlap(sector, index - 1ULL, start_lba, sectors) == 0) {
        ush_writeln_i18n("partctl: range overlaps an existing partition", "partctl: 范围与已有分区重叠");
        return 0;
    }

    if (boot != 0ULL) {
        for (i = 0ULL; i < PARTCTL_MBR_ENTRY_COUNT; i++) {
            u64 eoff = PARTCTL_MBR_ENTRY_OFFSET + (i * PARTCTL_MBR_ENTRY_SIZE);
            sector[eoff] = 0x00U;
        }
    }

    off = PARTCTL_MBR_ENTRY_OFFSET + ((index - 1ULL) * PARTCTL_MBR_ENTRY_SIZE);
    ush_zero((void *)&sector[off], PARTCTL_MBR_ENTRY_SIZE);
    sector[off + 0ULL] = (boot != 0ULL) ? 0x80U : 0x00U;
    sector[off + 1ULL] = 0xFFU;
    sector[off + 2ULL] = 0xFFU;
    sector[off + 3ULL] = 0xFFU;
    sector[off + 4ULL] = (unsigned char)type;
    sector[off + 5ULL] = 0xFFU;
    sector[off + 6ULL] = 0xFFU;
    sector[off + 7ULL] = 0xFFU;
    partctl_write_u32_le(&sector[off + 8ULL], (unsigned int)start_lba);
    partctl_write_u32_le(&sector[off + 12ULL], (unsigned int)sectors);
    partctl_mbr_ensure_signature(sector);

    if (partctl_write_sector0(sector) == 0) {
        return 0;
    }

    (void)printf((ush_locale_is_zh() != 0) ? "partctl: 已创建 (created) p%llu start=%llu sectors=%llu type=0x%02X boot=%llu\n"
                                           : "partctl: created p%llu start=%llu sectors=%llu type=0x%02X boot=%llu\n",
                 (unsigned long long)index, (unsigned long long)start_lba, (unsigned long long)sectors,
                 (unsigned int)((unsigned char)type), (unsigned long long)boot);
    return 1;
}

static int partctl_run(const char *arg) {
    char tokens[8][USH_ARG_MAX];
    int token_count = partctl_tokenize(arg, tokens, 8);

    if (token_count == 0 || ush_streq(tokens[0], "help") != 0) {
        partctl_usage();
        return 1;
    }

    if (ush_streq(tokens[0], "list") != 0) {
        return partctl_cmd_list();
    }

    if (ush_streq(tokens[0], "init-mbr") != 0) {
        return partctl_cmd_init_mbr();
    }

    if (ush_streq(tokens[0], "delete") != 0) {
        if (token_count != 2) {
            partctl_usage();
            return 0;
        }
        return partctl_cmd_delete(tokens[1]);
    }

    if (ush_streq(tokens[0], "set-boot") != 0) {
        if (token_count != 3) {
            partctl_usage();
            return 0;
        }
        return partctl_cmd_set_boot(tokens[1], tokens[2]);
    }

    if (ush_streq(tokens[0], "create") != 0) {
        if (token_count != 5 && token_count != 6) {
            partctl_usage();
            return 0;
        }
        return partctl_cmd_create(tokens[1], tokens[2], tokens[3], tokens[4], (token_count == 6) ? tokens[5] : "");
    }

    ush_writeln_i18n("partctl: unknown subcommand", "partctl: 未知子命令");
    partctl_usage();
    return 0;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "partctl") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = partctl_run(arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
