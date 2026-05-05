#include <sqlite3.h>

#include <cleonos_syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLEONOS_SQLITE_VFS_NAME "cleonos"
#define CLEONOS_SQLITE_PATH_MAX 192
#define CLEONOS_SQLITE_TEMP_PREFIX "/temp/sqlite-"

typedef struct cleonos_sqlite_file {
    sqlite3_file base;
    char *path;
    unsigned char *data;
    sqlite3_int64 size;
    sqlite3_int64 capacity;
    int flags;
    int dirty;
    int delete_on_close;
    int lock_level;
} cleonos_sqlite_file;

static int cleonos_sqlite_close(sqlite3_file *file);
static int cleonos_sqlite_read(sqlite3_file *file, void *out, int amount, sqlite3_int64 offset);
static int cleonos_sqlite_write(sqlite3_file *file, const void *data, int amount, sqlite3_int64 offset);
static int cleonos_sqlite_truncate(sqlite3_file *file, sqlite3_int64 size);
static int cleonos_sqlite_sync(sqlite3_file *file, int flags);
static int cleonos_sqlite_file_size(sqlite3_file *file, sqlite3_int64 *out_size);
static int cleonos_sqlite_lock(sqlite3_file *file, int level);
static int cleonos_sqlite_unlock(sqlite3_file *file, int level);
static int cleonos_sqlite_check_reserved_lock(sqlite3_file *file, int *out_result);
static int cleonos_sqlite_file_control(sqlite3_file *file, int op, void *arg);
static int cleonos_sqlite_sector_size(sqlite3_file *file);
static int cleonos_sqlite_device_characteristics(sqlite3_file *file);

static int cleonos_sqlite_open(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *file, int flags,
                               int *out_flags);
static int cleonos_sqlite_delete(sqlite3_vfs *vfs, const char *name, int sync_dir);
static int cleonos_sqlite_access(sqlite3_vfs *vfs, const char *name, int flags, int *out_result);
static int cleonos_sqlite_full_pathname(sqlite3_vfs *vfs, const char *name, int out_size, char *out_path);
static void *cleonos_sqlite_dl_open(sqlite3_vfs *vfs, const char *filename);
static void cleonos_sqlite_dl_error(sqlite3_vfs *vfs, int out_size, char *out_error);
static void (*cleonos_sqlite_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void);
static void cleonos_sqlite_dl_close(sqlite3_vfs *vfs, void *handle);
static int cleonos_sqlite_randomness(sqlite3_vfs *vfs, int out_size, char *out);
static int cleonos_sqlite_sleep(sqlite3_vfs *vfs, int microseconds);
static int cleonos_sqlite_current_time(sqlite3_vfs *vfs, double *out_days);
static int cleonos_sqlite_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *out_ms);
static int cleonos_sqlite_get_last_error(sqlite3_vfs *vfs, int out_size, char *out_error);

static const sqlite3_io_methods cleonos_sqlite_io_methods = {
    1,
    cleonos_sqlite_close,
    cleonos_sqlite_read,
    cleonos_sqlite_write,
    cleonos_sqlite_truncate,
    cleonos_sqlite_sync,
    cleonos_sqlite_file_size,
    cleonos_sqlite_lock,
    cleonos_sqlite_unlock,
    cleonos_sqlite_check_reserved_lock,
    cleonos_sqlite_file_control,
    cleonos_sqlite_sector_size,
    cleonos_sqlite_device_characteristics,
    0,
    0,
    0,
    0,
    0,
    0,
};

static int cleonos_sqlite_set_system_call(sqlite3_vfs *vfs, const char *name, sqlite3_syscall_ptr fn);
static sqlite3_syscall_ptr cleonos_sqlite_get_system_call(sqlite3_vfs *vfs, const char *name);
static const char *cleonos_sqlite_next_system_call(sqlite3_vfs *vfs, const char *name);

static sqlite3_vfs cleonos_sqlite_vfs = {
    3,
    sizeof(cleonos_sqlite_file),
    CLEONOS_SQLITE_PATH_MAX,
    0,
    CLEONOS_SQLITE_VFS_NAME,
    0,
    cleonos_sqlite_open,
    cleonos_sqlite_delete,
    cleonos_sqlite_access,
    cleonos_sqlite_full_pathname,
    cleonos_sqlite_dl_open,
    cleonos_sqlite_dl_error,
    cleonos_sqlite_dl_sym,
    cleonos_sqlite_dl_close,
    cleonos_sqlite_randomness,
    cleonos_sqlite_sleep,
    cleonos_sqlite_current_time,
    cleonos_sqlite_get_last_error,
    cleonos_sqlite_current_time_int64,
    cleonos_sqlite_set_system_call,
    cleonos_sqlite_get_system_call,
    cleonos_sqlite_next_system_call,
};


static int cleonos_sqlite_set_system_call(sqlite3_vfs *vfs, const char *name, sqlite3_syscall_ptr fn) {
    (void)vfs;
    (void)name;
    (void)fn;
    return SQLITE_NOTFOUND;
}

static sqlite3_syscall_ptr cleonos_sqlite_get_system_call(sqlite3_vfs *vfs, const char *name) {
    (void)vfs;
    (void)name;
    return (sqlite3_syscall_ptr)0;
}

static const char *cleonos_sqlite_next_system_call(sqlite3_vfs *vfs, const char *name) {
    (void)vfs;
    (void)name;
    return (const char *)0;
}

static unsigned long long cleonos_sqlite_temp_counter = 1ULL;

static size_t cleonos_sqlite_strlen(const char *text) {
    return (text == (const char *)0) ? 0U : strlen(text);
}

static char *cleonos_sqlite_strdup(const char *text) {
    size_t len;
    char *copy;

    if (text == (const char *)0) {
        return (char *)0;
    }

    len = cleonos_sqlite_strlen(text);
    copy = (char *)malloc(len + 1U);
    if (copy == (char *)0) {
        return (char *)0;
    }

    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

static int cleonos_sqlite_path_is_absolute(const char *path) {
    return (path != (const char *)0 && path[0] == '/') ? 1 : 0;
}

static int cleonos_sqlite_path_normalize(const char *name, char *out_path, size_t out_size) {
    size_t name_len;

    if (out_path == (char *)0 || out_size < 2U) {
        return SQLITE_CANTOPEN;
    }

    if (name == (const char *)0 || name[0] == '\0') {
        int wrote = snprintf(out_path, out_size, CLEONOS_SQLITE_TEMP_PREFIX "%llu.db",
                             (unsigned long long)cleonos_sqlite_temp_counter++);
        return (wrote > 0 && (size_t)wrote < out_size) ? SQLITE_OK : SQLITE_CANTOPEN;
    }

    name_len = cleonos_sqlite_strlen(name);
    if (cleonos_sqlite_path_is_absolute(name) != 0) {
        if (name_len + 1U > out_size) {
            return SQLITE_CANTOPEN;
        }
        memcpy(out_path, name, name_len + 1U);
        return SQLITE_OK;
    }

    if (name_len + 2U > out_size) {
        return SQLITE_CANTOPEN;
    }

    out_path[0] = '/';
    memcpy(out_path + 1U, name, name_len + 1U);
    return SQLITE_OK;
}

static int cleonos_sqlite_file_exists(const char *path) {
    u64 type;

    if (path == (const char *)0) {
        return 0;
    }

    type = cleonos_sys_fs_stat_type(path);
    return (type == 1ULL || type == 2ULL) ? 1 : 0;
}

static int cleonos_sqlite_ensure_parent_dirs(const char *path) {
    char work[CLEONOS_SQLITE_PATH_MAX + 1];
    size_t len;
    size_t i;

    if (path == (const char *)0) {
        return SQLITE_OK;
    }

    len = cleonos_sqlite_strlen(path);
    if (len == 0U || len > CLEONOS_SQLITE_PATH_MAX) {
        return SQLITE_CANTOPEN;
    }

    memcpy(work, path, len + 1U);
    for (i = 1U; i < len; i++) {
        if (work[i] != '/') {
            continue;
        }

        work[i] = '\0';
        if (work[0] != '\0' && cleonos_sys_fs_stat_type(work) != 2ULL && cleonos_sys_fs_mkdir(work) == 0ULL) {
            work[i] = '/';
            return SQLITE_CANTOPEN;
        }
        work[i] = '/';
    }

    return SQLITE_OK;
}

static int cleonos_sqlite_read_whole_file(const char *path, unsigned char **out_data, sqlite3_int64 *out_size) {
    u64 raw_size;
    u64 fd;
    unsigned char *buffer;
    size_t done;

    if (out_data == (unsigned char **)0 || out_size == (sqlite3_int64 *)0) {
        return SQLITE_IOERR;
    }

    *out_data = (unsigned char *)0;
    *out_size = 0;

    if (cleonos_sys_fs_stat_type(path) != 1ULL) {
        return SQLITE_CANTOPEN;
    }

    raw_size = cleonos_sys_fs_stat_size(path);
    if (raw_size > 0x7FFFFFFFULL) {
        return SQLITE_TOOBIG;
    }

    buffer = (unsigned char *)malloc((size_t)raw_size + 1U);
    if (buffer == (unsigned char *)0 && raw_size != 0ULL) {
        return SQLITE_NOMEM;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        free(buffer);
        return SQLITE_CANTOPEN;
    }

    done = 0U;
    while (done < (size_t)raw_size) {
        u64 got = cleonos_sys_fd_read(fd, buffer + done, raw_size - (u64)done);
        if (got == 0ULL || got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            free(buffer);
            return SQLITE_IOERR_READ;
        }
        done += (size_t)got;
    }

    (void)cleonos_sys_fd_close(fd);
    *out_data = buffer;
    *out_size = (sqlite3_int64)raw_size;
    return SQLITE_OK;
}

static int cleonos_sqlite_write_whole_file(const char *path, const unsigned char *data, sqlite3_int64 size) {
    u64 fd;
    sqlite3_int64 done;

    if (path == (const char *)0 || size < 0) {
        return SQLITE_IOERR_WRITE;
    }

    if (cleonos_sqlite_ensure_parent_dirs(path) != SQLITE_OK) {
        return SQLITE_CANTOPEN;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_WRONLY | CLEONOS_O_CREAT | CLEONOS_O_TRUNC, 0ULL);
    if (fd == (u64)-1) {
        return SQLITE_CANTOPEN;
    }

    done = 0;
    while (done < size) {
        u64 wrote = cleonos_sys_fd_write(fd, data + done, (u64)(size - done));
        if (wrote == 0ULL || wrote == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            return SQLITE_IOERR_WRITE;
        }
        done += (sqlite3_int64)wrote;
    }

    (void)cleonos_sys_fd_close(fd);
    return SQLITE_OK;
}

static int cleonos_sqlite_reserve(cleonos_sqlite_file *file, sqlite3_int64 need) {
    sqlite3_int64 capacity;
    unsigned char *grown;

    if (file == (cleonos_sqlite_file *)0 || need < 0) {
        return SQLITE_IOERR;
    }

    if (need <= file->capacity) {
        return SQLITE_OK;
    }

    capacity = (file->capacity > 0) ? file->capacity : 4096;
    while (capacity < need) {
        sqlite3_int64 next = capacity * 2;
        if (next <= capacity) {
            capacity = need;
            break;
        }
        capacity = next;
    }

    if ((size_t)capacity < (size_t)need) {
        return SQLITE_TOOBIG;
    }

    grown = (unsigned char *)realloc(file->data, (size_t)capacity);
    if (grown == (unsigned char *)0) {
        return SQLITE_NOMEM;
    }

    if (capacity > file->capacity) {
        memset(grown + file->capacity, 0, (size_t)(capacity - file->capacity));
    }

    file->data = grown;
    file->capacity = capacity;
    return SQLITE_OK;
}

static int cleonos_sqlite_flush(cleonos_sqlite_file *file) {
    if (file == (cleonos_sqlite_file *)0 || file->dirty == 0 || file->path == (char *)0) {
        return SQLITE_OK;
    }

    if (cleonos_sqlite_write_whole_file(file->path, file->data, file->size) != SQLITE_OK) {
        return SQLITE_IOERR_WRITE;
    }

    file->dirty = 0;
    return SQLITE_OK;
}

static int cleonos_sqlite_close(sqlite3_file *base_file) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    int rc = SQLITE_OK;

    if (file == (cleonos_sqlite_file *)0) {
        return SQLITE_OK;
    }

    if (file->delete_on_close == 0) {
        rc = cleonos_sqlite_flush(file);
    }

    if (file->delete_on_close != 0 && file->path != (char *)0) {
        (void)cleonos_sys_fs_remove(file->path);
    }

    free(file->data);
    free(file->path);
    memset(file, 0, sizeof(*file));
    return rc;
}

static int cleonos_sqlite_read(sqlite3_file *base_file, void *out, int amount, sqlite3_int64 offset) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;

    if (file == (cleonos_sqlite_file *)0 || out == (void *)0 || amount < 0 || offset < 0) {
        return SQLITE_IOERR_READ;
    }

    if (offset >= file->size) {
        memset(out, 0, (size_t)amount);
        return SQLITE_IOERR_SHORT_READ;
    }

    if (offset + amount > file->size) {
        sqlite3_int64 available = file->size - offset;
        memcpy(out, file->data + offset, (size_t)available);
        memset((unsigned char *)out + available, 0, (size_t)(amount - available));
        return SQLITE_IOERR_SHORT_READ;
    }

    memcpy(out, file->data + offset, (size_t)amount);
    return SQLITE_OK;
}

static int cleonos_sqlite_write(sqlite3_file *base_file, const void *data, int amount, sqlite3_int64 offset) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    sqlite3_int64 end;
    int rc;

    if (file == (cleonos_sqlite_file *)0 || data == (const void *)0 || amount < 0 || offset < 0) {
        return SQLITE_IOERR_WRITE;
    }

    end = offset + amount;
    rc = cleonos_sqlite_reserve(file, end);
    if (rc != SQLITE_OK) {
        return rc;
    }

    if (offset > file->size) {
        memset(file->data + file->size, 0, (size_t)(offset - file->size));
    }

    memcpy(file->data + offset, data, (size_t)amount);
    if (end > file->size) {
        file->size = end;
    }
    file->dirty = 1;
    return SQLITE_OK;
}

static int cleonos_sqlite_truncate(sqlite3_file *base_file, sqlite3_int64 size) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    int rc;

    if (file == (cleonos_sqlite_file *)0 || size < 0) {
        return SQLITE_IOERR_TRUNCATE;
    }

    rc = cleonos_sqlite_reserve(file, size);
    if (rc != SQLITE_OK) {
        return rc;
    }

    if (size > file->size) {
        memset(file->data + file->size, 0, (size_t)(size - file->size));
    }

    file->size = size;
    file->dirty = 1;
    return SQLITE_OK;
}

static int cleonos_sqlite_sync(sqlite3_file *base_file, int flags) {
    (void)flags;
    return cleonos_sqlite_flush((cleonos_sqlite_file *)base_file);
}

static int cleonos_sqlite_file_size(sqlite3_file *base_file, sqlite3_int64 *out_size) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;

    if (file == (cleonos_sqlite_file *)0 || out_size == (sqlite3_int64 *)0) {
        return SQLITE_IOERR_FSTAT;
    }

    *out_size = file->size;
    return SQLITE_OK;
}

static int cleonos_sqlite_lock(sqlite3_file *base_file, int level) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    if (file != (cleonos_sqlite_file *)0) {
        file->lock_level = level;
    }
    return SQLITE_OK;
}

static int cleonos_sqlite_unlock(sqlite3_file *base_file, int level) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    if (file != (cleonos_sqlite_file *)0) {
        file->lock_level = level;
    }
    return SQLITE_OK;
}

static int cleonos_sqlite_check_reserved_lock(sqlite3_file *base_file, int *out_result) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    if (out_result == (int *)0) {
        return SQLITE_IOERR_CHECKRESERVEDLOCK;
    }
    *out_result = (file != (cleonos_sqlite_file *)0 && file->lock_level >= SQLITE_LOCK_RESERVED) ? 1 : 0;
    return SQLITE_OK;
}

static int cleonos_sqlite_file_control(sqlite3_file *base_file, int op, void *arg) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;

    if (op == SQLITE_FCNTL_LOCKSTATE && arg != (void *)0) {
        *(int *)arg = (file != (cleonos_sqlite_file *)0) ? file->lock_level : SQLITE_LOCK_NONE;
        return SQLITE_OK;
    }

    if (op == SQLITE_FCNTL_SYNC || op == SQLITE_FCNTL_COMMIT_PHASETWO) {
        return SQLITE_OK;
    }

    return SQLITE_NOTFOUND;
}

static int cleonos_sqlite_sector_size(sqlite3_file *base_file) {
    (void)base_file;
    return 4096;
}

static int cleonos_sqlite_device_characteristics(sqlite3_file *base_file) {
    (void)base_file;
    return SQLITE_IOCAP_SAFE_APPEND | SQLITE_IOCAP_POWERSAFE_OVERWRITE;
}

static int cleonos_sqlite_open(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *base_file, int flags,
                               int *out_flags) {
    cleonos_sqlite_file *file = (cleonos_sqlite_file *)base_file;
    char normalized[CLEONOS_SQLITE_PATH_MAX + 1];
    unsigned char *contents = (unsigned char *)0;
    sqlite3_int64 size = 0;
    int rc;

    (void)vfs;

    if (file == (cleonos_sqlite_file *)0) {
        return SQLITE_CANTOPEN;
    }

    memset(file, 0, sizeof(*file));
    rc = cleonos_sqlite_path_normalize((const char *)name, normalized, sizeof(normalized));
    if (rc != SQLITE_OK) {
        return rc;
    }

    file->path = cleonos_sqlite_strdup(normalized);
    if (file->path == (char *)0) {
        return SQLITE_NOMEM;
    }

    file->flags = flags;
    file->delete_on_close = ((flags & SQLITE_OPEN_DELETEONCLOSE) != 0) ? 1 : 0;
    file->base.pMethods = &cleonos_sqlite_io_methods;

    if (cleonos_sys_fs_stat_type(file->path) == 1ULL) {
        rc = cleonos_sqlite_read_whole_file(file->path, &contents, &size);
        if (rc != SQLITE_OK) {
            cleonos_sqlite_close(base_file);
            return rc;
        }

        file->data = contents;
        file->size = size;
        file->capacity = size;
    } else if ((flags & SQLITE_OPEN_CREATE) == 0) {
        cleonos_sqlite_close(base_file);
        return SQLITE_CANTOPEN;
    }

    if (out_flags != (int *)0) {
        *out_flags = flags;
    }
    return SQLITE_OK;
}

static int cleonos_sqlite_delete(sqlite3_vfs *vfs, const char *name, int sync_dir) {
    char normalized[CLEONOS_SQLITE_PATH_MAX + 1];
    (void)vfs;
    (void)sync_dir;

    if (cleonos_sqlite_path_normalize(name, normalized, sizeof(normalized)) != SQLITE_OK) {
        return SQLITE_IOERR_DELETE;
    }

    if (cleonos_sqlite_file_exists(normalized) == 0) {
        return SQLITE_OK;
    }

    return (cleonos_sys_fs_remove(normalized) != 0ULL) ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int cleonos_sqlite_access(sqlite3_vfs *vfs, const char *name, int flags, int *out_result) {
    char normalized[CLEONOS_SQLITE_PATH_MAX + 1];
    u64 type;

    (void)vfs;
    (void)flags;

    if (out_result == (int *)0) {
        return SQLITE_IOERR_ACCESS;
    }

    *out_result = 0;
    if (cleonos_sqlite_path_normalize(name, normalized, sizeof(normalized)) != SQLITE_OK) {
        return SQLITE_OK;
    }

    type = cleonos_sys_fs_stat_type(normalized);
    *out_result = (type == 1ULL || type == 2ULL) ? 1 : 0;
    return SQLITE_OK;
}

static int cleonos_sqlite_full_pathname(sqlite3_vfs *vfs, const char *name, int out_size, char *out_path) {
    (void)vfs;
    if (out_path == (char *)0 || out_size <= 0) {
        return SQLITE_CANTOPEN;
    }
    return cleonos_sqlite_path_normalize(name, out_path, (size_t)out_size);
}

static void *cleonos_sqlite_dl_open(sqlite3_vfs *vfs, const char *filename) {
    (void)vfs;
    (void)filename;
    return (void *)0;
}

static void cleonos_sqlite_dl_error(sqlite3_vfs *vfs, int out_size, char *out_error) {
    (void)vfs;
    if (out_error != (char *)0 && out_size > 0) {
        snprintf(out_error, (size_t)out_size, "loadable extensions disabled");
    }
}

static void (*cleonos_sqlite_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void) {
    (void)vfs;
    (void)handle;
    (void)symbol;
    return (void (*)(void))0;
}

static void cleonos_sqlite_dl_close(sqlite3_vfs *vfs, void *handle) {
    (void)vfs;
    (void)handle;
}

static int cleonos_sqlite_randomness(sqlite3_vfs *vfs, int out_size, char *out) {
    unsigned long long seed;
    int i;
    (void)vfs;

    if (out == (char *)0 || out_size <= 0) {
        return 0;
    }

    seed = (unsigned long long)cleonos_sys_time_ms() ^ 0x9E3779B97F4A7C15ULL;
    for (i = 0; i < out_size; i++) {
        seed ^= seed << 7;
        seed ^= seed >> 9;
        seed ^= seed << 8;
        out[i] = (char)(seed & 0xFFU);
    }
    return out_size;
}

static int cleonos_sqlite_sleep(sqlite3_vfs *vfs, int microseconds) {
    u64 ms;
    (void)vfs;

    if (microseconds <= 0) {
        return 0;
    }

    ms = (u64)((microseconds + 999) / 1000);
    if (ms == 0ULL) {
        ms = 1ULL;
    }
    (void)cleonos_sys_sleep_ms(ms);
    return (int)(ms * 1000ULL);
}

static int cleonos_sqlite_current_time(sqlite3_vfs *vfs, double *out_days) {
    u64 ms_since_boot;
    const double julian_unix_epoch = 2440587.5;
    (void)vfs;

    if (out_days == (double *)0) {
        return SQLITE_IOERR;
    }

    ms_since_boot = cleonos_sys_time_ms();
    *out_days = julian_unix_epoch + ((double)ms_since_boot / 86400000.0);
    return SQLITE_OK;
}

static int cleonos_sqlite_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *out_ms) {
    double days;

    if (out_ms == (sqlite3_int64 *)0) {
        return SQLITE_IOERR;
    }

    if (cleonos_sqlite_current_time(vfs, &days) != SQLITE_OK) {
        return SQLITE_IOERR;
    }

    *out_ms = (sqlite3_int64)(days * 86400000.0);
    return SQLITE_OK;
}

static int cleonos_sqlite_get_last_error(sqlite3_vfs *vfs, int out_size, char *out_error) {
    (void)vfs;
    if (out_error != (char *)0 && out_size > 0) {
        out_error[0] = '\0';
    }
    return 0;
}

int sqlite3_os_init(void) {
    return sqlite3_vfs_register(&cleonos_sqlite_vfs, 1);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}
