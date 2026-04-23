#ifndef CLKS_DISK_H
#define CLKS_DISK_H

#include <clks/types.h>

#define CLKS_DISK_SECTOR_SIZE 512ULL
#define CLKS_DISK_PATH_MAX 192U
#define CLKS_DISK_NODE_FILE 1ULL
#define CLKS_DISK_NODE_DIR 2ULL

void clks_disk_init(void);

clks_bool clks_disk_present(void);
u64 clks_disk_size_bytes(void);
u64 clks_disk_sector_count(void);

clks_bool clks_disk_read_sector(u64 lba, void *out_sector);
clks_bool clks_disk_write_sector(u64 lba, const void *sector_data);

clks_bool clks_disk_is_formatted_fat32(void);
clks_bool clks_disk_format_fat32(const char *label);

clks_bool clks_disk_mount(const char *mount_path);
clks_bool clks_disk_is_mounted(void);
const char *clks_disk_mount_path(void);
clks_bool clks_disk_path_in_mount(const char *path);

clks_bool clks_disk_stat(const char *path, u64 *out_type, u64 *out_size);
const void *clks_disk_read_all(const char *path, u64 *out_size);
u64 clks_disk_count_children(const char *dir_path);
clks_bool clks_disk_get_child_name(const char *dir_path, u64 index, char *out_name, usize out_name_size);
clks_bool clks_disk_mkdir(const char *path);
clks_bool clks_disk_write_all(const char *path, const void *data, u64 size);
clks_bool clks_disk_append(const char *path, const void *data, u64 size);
clks_bool clks_disk_remove(const char *path);
u64 clks_disk_node_count(void);

#endif
