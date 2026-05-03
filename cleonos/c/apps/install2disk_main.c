#include "cmd_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INSTALL_MOUNT_PATH "/temp/disk"
#define INSTALL_SECTOR_SIZE 512ULL
#define INSTALL_STAGE2_LBA 1ULL
#define INSTALL_SKIP_PREFIX INSTALL_MOUNT_PATH
#define INSTALL_COPY_CHUNK_SIZE 8192U
#define INSTALL_WHOLE_FILE_LIMIT (128ULL * 1024ULL)
#define INSTALL_PROGRESS_BAR_WIDTH 28U
#define INSTALL_PROGRESS_STEP_PERCENT 5ULL
#define INSTALL_KERNEL_SOURCE "/system/install/clks_kernel.elf"
#define INSTALL_KERNEL_TARGET INSTALL_MOUNT_PATH "/boot/clks_kernel.elf"

typedef struct install_progress {
    const char *label;
    u64 total_items;
    u64 total_bytes;
    u64 done_items;
    u64 done_bytes;
    u64 last_percent;
    int started;
} install_progress;

static void install_stage(const char *name) {
    (void)printf("install2disk: == %s ==\n", name);
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

    (void)printf("install2disk: %s [%s] %3llu%% %llu/%llu items %llu/%llu bytes\n",
                 (progress->label != (const char *)0) ? progress->label : "progress", bar,
                 (unsigned long long)percent, (unsigned long long)progress->done_items,
                 (unsigned long long)progress->total_items, (unsigned long long)progress->done_bytes,
                 (unsigned long long)progress->total_bytes);
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
        (void)printf("install2disk: mkdir failed: %s\n", path);
        return 0;
    }

    return 1;
}

static int install_file_exists(const char *path) {
    return (cleonos_sys_fs_stat_type(path) == 1ULL) ? 1 : 0;
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
    u64 got;
    u64 wrote;
    u64 src_fd;
    u64 file_offset = 0ULL;

    size = cleonos_sys_fs_stat_size(src);
    if (size == (u64)-1) {
        (void)printf("install2disk: stat failed: %s\n", src);
        return 0;
    }

    if (size == 0ULL) {
        if (cleonos_sys_fs_write(dst, "", 0ULL) == 0ULL) {
            (void)printf("install2disk: create failed: %s\n", dst);
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
                (void)printf("install2disk: open read failed: %s\n", src);
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
                    (void)printf("install2disk: read failed: %s\n", src);
                    (void)printf("install2disk: src=%s mode=whole offset=%llu want=%llu got=%llu size=%llu\n", src,
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
                (void)printf("install2disk: write failed: %s\n", dst);
                (void)printf("install2disk: src=%s mode=whole size=%llu wrote=%llu\n", src,
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
            return 1;
        }

        (void)printf("install2disk: whole-file buffer unavailable, falling back: %s\n", src);
    }

    src_fd = cleonos_sys_fd_open(src, CLEONOS_O_RDONLY, 0ULL);
    if (src_fd == (u64)-1) {
        (void)printf("install2disk: open read failed: %s\n", src);
        return 0;
    }

    if (cleonos_sys_fs_write(dst, "", 0ULL) == 0ULL) {
        (void)cleonos_sys_fd_close(src_fd);
        (void)printf("install2disk: create failed: %s\n", dst);
        return 0;
    }

    for (;;) {
        got = cleonos_sys_fd_read(src_fd, buffer, (u64)sizeof(buffer));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(src_fd);
            (void)printf("install2disk: read failed: %s\n", src);
            (void)printf("install2disk: src=%s mode=stream offset=%llu want=%llu got=%llu size=%llu\n", src,
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
            (void)printf("install2disk: write failed: %s\n", dst);
            (void)printf("install2disk: src=%s offset=%llu chunk=%llu wrote=%llu size=%llu\n", src,
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
        (void)printf("install2disk: directory depth too large while scanning: %s\n", src);
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
            (void)printf("install2disk: path too long while scanning near %s\n", name);
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
        (void)printf("install2disk: directory depth too large: %s\n", src);
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
            (void)printf("install2disk: path too long near %s\n", name);
            return 0;
        }

        if (install_copy_tree(child_src, child_dst, depth + 1ULL, copied_files, copied_bytes, progress) == 0) {
            return 0;
        }
    }

    return 1;
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
        (void)printf("install2disk: open boot blob failed: %s\n", src_path);
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
            (void)printf("install2disk: read sector failed: %llu\n", (unsigned long long)sector_lba);
            return 0;
        }

        for (i = 0ULL; i < chunk; i++) {
            u64 p = want_pos + i;
            if (p < cached_start || p >= cached_start + cached_len) {
                if (p < file_pos) {
                    (void)cleonos_sys_fd_close(fd);
                    (void)printf("install2disk: non-sequential boot blob read\n");
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
                        (void)printf("install2disk: boot blob seek failed\n");
                        return 0;
                    }
                    file_pos += got;
                }

                cached_start = file_pos;
                cached_len = cleonos_sys_fd_read(fd, file_buf, (u64)sizeof(file_buf));
                if (cached_len == (u64)-1 || cached_len == 0ULL) {
                    (void)cleonos_sys_fd_close(fd);
                    (void)printf("install2disk: boot blob read failed\n");
                    return 0;
                }
                file_pos += cached_len;
            }

            sector[sector_off + i] = (unsigned char)file_buf[p - cached_start];
        }

        if (cleonos_sys_disk_write_sector(sector_lba, sector) == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            (void)printf("install2disk: write sector failed: %llu\n", (unsigned long long)sector_lba);
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

    hdd_size = cleonos_sys_fs_stat_size("/system/install/limine-bios-hdd.bin");
    stage2_size = cleonos_sys_fs_stat_size("/system/install/limine-bios.sys");
    if (hdd_size == (u64)-1 || hdd_size < INSTALL_SECTOR_SIZE || stage2_size == (u64)-1 || stage2_size == 0ULL) {
        (void)printf("install2disk: missing Limine install blobs\n");
        return 0;
    }

    if (install_raw_write_bytes(0ULL, "/system/install/limine-bios-hdd.bin", 0ULL, INSTALL_SECTOR_SIZE, progress) ==
        0) {
        return 0;
    }

    stage2_bytes = hdd_size - INSTALL_SECTOR_SIZE;
    if (stage2_bytes > 0ULL && install_raw_write_bytes(INSTALL_STAGE2_LBA, "/system/install/limine-bios-hdd.bin",
                                                       INSTALL_SECTOR_SIZE, stage2_bytes, progress) == 0) {
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
        (void)printf("install2disk: invalid disk sector count: %llu\n", (unsigned long long)sector_count);
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
    if (install_mkdir(INSTALL_MOUNT_PATH "/boot") == 0 || install_mkdir(INSTALL_MOUNT_PATH "/boot/limine") == 0) {
        return 0;
    }

    if (install_copy_file(INSTALL_KERNEL_SOURCE, INSTALL_KERNEL_TARGET, copied_files, copied_bytes, progress) == 0) {
        return 0;
    }
    if (install_copy_file("/system/install/limine-bios.sys", INSTALL_MOUNT_PATH "/boot/limine/limine-bios.sys",
                          copied_files, copied_bytes, progress) == 0) {
        return 0;
    }
    if (install_copy_file("/system/install/limine-harddisk.conf", INSTALL_MOUNT_PATH "/boot/limine/limine.conf",
                          copied_files, copied_bytes, progress) == 0 ||
        install_copy_file("/system/install/limine-harddisk.conf", INSTALL_MOUNT_PATH "/limine.conf", copied_files,
                          copied_bytes, progress) == 0) {
        return 0;
    }

    return 1;
}

static int install_update_kernel_only(void) {
    u64 copied_files = 0ULL;
    u64 copied_bytes = 0ULL;

    install_stage("update kernel");
    if (install_mkdir(INSTALL_MOUNT_PATH "/boot") == 0) {
        return 0;
    }

    if (install_copy_file(INSTALL_KERNEL_SOURCE, INSTALL_KERNEL_TARGET, &copied_files, &copied_bytes,
                          (install_progress *)0) == 0) {
        return 0;
    }

    (void)printf("install2disk: kernel updated: %llu bytes\n", (unsigned long long)copied_bytes);
    (void)puts("install2disk: done. Reboot or use make run-hardboot.");
    return 1;
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

static int install2disk_run(void) {
    u64 copied_files = 0ULL;
    u64 copied_bytes = 0ULL;
    install_progress root_progress;

    memset(&root_progress, 0, sizeof(root_progress));
    root_progress.label = "copy root filesystem";

    if (cleonos_sys_disk_present() == 0ULL) {
        (void)puts("install2disk: disk not present");
        return 0;
    }

    if (install_mount_existing_disk() != 0 && install_file_exists(INSTALL_KERNEL_TARGET) != 0) {
        int choice;

        (void)puts("install2disk: existing CLeonOS hard-disk install detected.");
        choice = install_prompt_choice(
            "install2disk: choose [u]pdate kernel only, [f]ormat full install, [c]ancel (default: u): ", "ufc", 'u');

        if (choice == 'c') {
            (void)puts("install2disk: cancelled");
            return 0;
        }

        if (choice == 'u') {
            return install_update_kernel_only();
        }
    }

    install_stage("scan root filesystem");
    if (install_scan_tree("/", 0ULL, &root_progress) == 0) {
        return 0;
    }
    (void)printf("install2disk: rootfs plan %llu items, %llu bytes\n",
                 (unsigned long long)root_progress.total_items, (unsigned long long)root_progress.total_bytes);

    install_stage("format FAT32");
    if (cleonos_sys_disk_format_fat32("CLEONOS") == 0ULL) {
        (void)puts("install2disk: format failed");
        return 0;
    }

    install_stage("mount target disk");
    if (cleonos_sys_disk_mount(INSTALL_MOUNT_PATH) == 0ULL) {
        (void)puts("install2disk: mount failed");
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

    install_stage("write Limine BIOS boot stages");
    if (install_limine_bios((install_progress *)0) == 0) {
        return 0;
    }

    (void)printf("install2disk: installed %llu files, %llu bytes\n", (unsigned long long)copied_files,
                 (unsigned long long)copied_bytes);
    (void)puts("install2disk: done. Use make run-hardboot to boot from disk.");
    return 1;
}

int cleonos_app_main(void) {
    return (install2disk_run() != 0) ? 0 : 1;
}
