#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../../include/cleonos_syscall.h"

#define DG_HEAP_SIZE (32U * 1024U * 1024U)
#define DG_MAX_MEM_FD 64
#define DG_PATH_MAX 192
#define DG_ENV_LINE_MAX 256
#define DG_STDIO_MAGIC 0x44474D46U
#define DG_SERIAL_LOG_CHUNK 176U
#define DG_SERIAL_LOG_PREFIX "[DOOM] "

struct dg_alloc_hdr {
    size_t size;
    int free;
    struct dg_alloc_hdr *next;
    struct dg_alloc_hdr *prev;
};

struct dg_mem_fd {
    int used;
    int writable;
    int dirty;
    size_t pos;
    size_t size;
    size_t cap;
    unsigned char *data;
    char path[DG_PATH_MAX];
};

struct dg_stream {
    unsigned int magic;
    int fd;
    int eof_flag;
    int err_flag;
};

static unsigned char g_dg_heap[DG_HEAP_SIZE];
static struct dg_alloc_hdr *g_dg_heap_head = (struct dg_alloc_hdr *)0;
static int g_dg_heap_ready = 0;
static struct dg_mem_fd g_dg_fds[DG_MAX_MEM_FD];
static struct dg_stream g_dg_stdin_stream = {DG_STDIO_MAGIC, 0, 0, 0};
static struct dg_stream g_dg_stdout_stream = {DG_STDIO_MAGIC, 1, 0, 0};
static struct dg_stream g_dg_stderr_stream = {DG_STDIO_MAGIC, 2, 0, 0};
static int g_dg_errno = 0;
static unsigned short g_dg_ctype_table[384];
static int g_dg_ctype_ready = 0;

FILE *stdin = (FILE *)(void *)&g_dg_stdin_stream;
FILE *stdout = (FILE *)(void *)&g_dg_stdout_stream;
FILE *stderr = (FILE *)(void *)&g_dg_stderr_stream;

int *__errno_location(void) {
    return &g_dg_errno;
}

static int dg_is_digit(int ch) {
    return (ch >= '0' && ch <= '9') ? 1 : 0;
}

static int dg_is_upper(int ch) {
    return (ch >= 'A' && ch <= 'Z') ? 1 : 0;
}

static int dg_is_lower(int ch) {
    return (ch >= 'a' && ch <= 'z') ? 1 : 0;
}

static int dg_is_alpha(int ch) {
    return (dg_is_upper(ch) != 0 || dg_is_lower(ch) != 0) ? 1 : 0;
}

static int dg_is_xdigit(int ch) {
    return (dg_is_digit(ch) != 0 || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) ? 1 : 0;
}

static int dg_is_space_char(int ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f') ? 1 : 0;
}

static int dg_to_lower_ascii(int ch) {
    return (dg_is_upper(ch) != 0) ? (ch - 'A' + 'a') : ch;
}

static void dg_init_ctype_table(void) {
    int c;

    if (g_dg_ctype_ready != 0) {
        return;
    }

    for (c = -128; c < 256; c++) {
        unsigned short flags = 0U;
        int ch = (c < 0) ? (c + 256) : c;

        if (dg_is_upper(ch) != 0) {
            flags |= _ISupper;
        }
        if (dg_is_lower(ch) != 0) {
            flags |= _ISlower;
        }
        if (dg_is_alpha(ch) != 0) {
            flags |= _ISalpha;
        }
        if (dg_is_digit(ch) != 0) {
            flags |= _ISdigit;
        }
        if (dg_is_xdigit(ch) != 0) {
            flags |= _ISxdigit;
        }
        if (dg_is_space_char(ch) != 0) {
            flags |= _ISspace;
        }
        if (ch == ' ' || ch == '\t') {
            flags |= _ISblank;
        }
        if (ch < 32 || ch == 127) {
            flags |= _IScntrl;
        }
        if (ch >= 32 && ch <= 126) {
            flags |= _ISprint;
        }
        if (ch >= 33 && ch <= 126) {
            flags |= _ISgraph;
        }
        if ((flags & (_ISalpha | _ISdigit)) != 0U) {
            flags |= _ISalnum;
        }
        if ((flags & (_ISgraph | _ISalnum)) != 0U && (flags & _ISalnum) == 0U) {
            flags |= _ISpunct;
        }

        g_dg_ctype_table[c + 128] = flags;
    }

    g_dg_ctype_ready = 1;
}

const unsigned short int **__ctype_b_loc(void) {
    static const unsigned short int *table_ptr = (const unsigned short int *)(void *)(g_dg_ctype_table + 128);
    dg_init_ctype_table();
    return &table_ptr;
}

int system(const char *command) {
    (void)command;
    errno = ENOSYS;
    return -1;
}

static size_t dg_strlen(const char *text) {
    size_t len = 0U;

    if (text == (const char *)0) {
        return 0U;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static void dg_copy(char *dst, size_t dst_size, const char *src) {
    size_t i = 0U;

    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }

    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1U < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static int dg_starts_with(const char *text, const char *prefix) {
    size_t i = 0U;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static int dg_path_exists(const char *path) {
    return (cleonos_sys_fs_stat_type(path) == (u64)-1) ? 0 : 1;
}

static void dg_join_path(char *out_path, size_t out_size, const char *dir, const char *name) {
    size_t dlen;
    size_t i = 0U;

    if (out_path == (char *)0 || out_size == 0U) {
        return;
    }

    out_path[0] = '\0';

    if (dir == (const char *)0 || name == (const char *)0) {
        return;
    }

    dlen = dg_strlen(dir);

    while (i < dlen && i + 1U < out_size) {
        out_path[i] = dir[i];
        i++;
    }

    if (i > 0U && out_path[i - 1U] != '/' && i + 1U < out_size) {
        out_path[i++] = '/';
    }

    {
        size_t j = 0U;
        while (name[j] != '\0' && i + 1U < out_size) {
            out_path[i++] = name[j++];
        }
    }

    out_path[i] = '\0';
}

static void dg_resolve_read_path(const char *path, char *out_path, size_t out_size) {
    char candidate[DG_PATH_MAX];

    if (path == (const char *)0 || out_path == (char *)0 || out_size == 0U) {
        return;
    }

    if (path[0] == '/') {
        dg_copy(out_path, out_size, path);
        return;
    }

    dg_join_path(candidate, sizeof(candidate), "/temp", path);
    if (dg_path_exists(candidate) != 0) {
        dg_copy(out_path, out_size, candidate);
        return;
    }

    dg_join_path(candidate, sizeof(candidate), "/shell", path);
    if (dg_path_exists(candidate) != 0) {
        dg_copy(out_path, out_size, candidate);
        return;
    }

    dg_join_path(candidate, sizeof(candidate), "/", path);
    dg_copy(out_path, out_size, candidate);
}

static void dg_resolve_write_path(const char *path, char *out_path, size_t out_size) {
    if (path == (const char *)0 || out_path == (char *)0 || out_size == 0U) {
        return;
    }

    if (path[0] == '/') {
        dg_copy(out_path, out_size, path);
        return;
    }

    dg_join_path(out_path, out_size, "/temp", path);
}

static int dg_is_temp_path(const char *path) {
    return dg_starts_with(path, "/temp");
}

static int dg_fd_slot_from_fd(int fd) {
    int slot = fd - 3;

    if (slot < 0 || slot >= DG_MAX_MEM_FD) {
        return -1;
    }

    if (g_dg_fds[slot].used == 0) {
        return -1;
    }

    return slot;
}

static int dg_fd_alloc_slot(void) {
    int i;

    for (i = 0; i < DG_MAX_MEM_FD; i++) {
        if (g_dg_fds[i].used == 0) {
            g_dg_fds[i].used = 1;
            g_dg_fds[i].writable = 0;
            g_dg_fds[i].dirty = 0;
            g_dg_fds[i].pos = 0U;
            g_dg_fds[i].size = 0U;
            g_dg_fds[i].cap = 0U;
            g_dg_fds[i].data = (unsigned char *)0;
            g_dg_fds[i].path[0] = '\0';
            return i;
        }
    }

    return -1;
}

static int dg_fd_ensure_cap(struct dg_mem_fd *file, size_t need) {
    unsigned char *new_buf;
    size_t new_cap;

    if (file == (struct dg_mem_fd *)0) {
        errno = EINVAL;
        return -1;
    }

    if (need <= file->cap) {
        return 0;
    }

    new_cap = (file->cap == 0U) ? 256U : file->cap;
    while (new_cap < need) {
        if (new_cap > ((size_t)-1) / 2U) {
            errno = ENOMEM;
            return -1;
        }
        new_cap *= 2U;
    }

    new_buf = (unsigned char *)realloc(file->data, new_cap);
    if (new_buf == (unsigned char *)0) {
        errno = ENOMEM;
        return -1;
    }

    file->data = new_buf;
    file->cap = new_cap;
    return 0;
}

static int dg_fd_flush_slot(struct dg_mem_fd *file) {
    u64 wrote;

    if (file == (struct dg_mem_fd *)0 || file->used == 0) {
        return 0;
    }

    if (file->writable == 0 || file->dirty == 0) {
        return 0;
    }

    if (dg_is_temp_path(file->path) == 0) {
        errno = EACCES;
        return -1;
    }

    wrote = cleonos_sys_fs_write(file->path, (const char *)file->data, (u64)file->size);
    if (wrote != (u64)file->size) {
        errno = EIO;
        return -1;
    }

    file->dirty = 0;
    return 0;
}

static void dg_serial_log_bytes(const void *buffer, size_t size) {
    const unsigned char *src = (const unsigned char *)buffer;
    static const char prefix[] = DG_SERIAL_LOG_PREFIX;
    char line[DG_SERIAL_LOG_CHUNK + sizeof(prefix) + 1U];
    size_t prefix_len = sizeof(prefix) - 1U;
    size_t offset = 0U;
    int first = 1;

    if (src == (const unsigned char *)0 || size == 0U) {
        return;
    }

    while (offset < size) {
        size_t room = DG_SERIAL_LOG_CHUNK;
        size_t used = 0U;

        if (first != 0 && prefix_len < sizeof(line)) {
            memcpy(line, prefix, prefix_len);
            used = prefix_len;
            first = 0;
        }

        while (offset < size && room > 0U) {
            unsigned char ch = src[offset++];

            if (ch == '\0') {
                ch = ' ';
            } else if (ch < 32U && ch != '\n' && ch != '\r' && ch != '\t') {
                ch = ' ';
            }

            line[used++] = (char)ch;
            room--;
        }

        if (used > 0U) {
            (void)cleonos_sys_log_write(line, (u64)used);
        }
    }
}

void *malloc(size_t size) {
    struct dg_alloc_hdr *cur;
    size_t aligned;

    if (size == 0U) {
        size = 1U;
    }

    aligned = (size + 15U) & ~(size_t)15U;

    if (g_dg_heap_ready == 0) {
        if (DG_HEAP_SIZE <= sizeof(struct dg_alloc_hdr) + 16U) {
            errno = ENOMEM;
            return (void *)0;
        }

        g_dg_heap_head = (struct dg_alloc_hdr *)(void *)g_dg_heap;
        g_dg_heap_head->size = DG_HEAP_SIZE - sizeof(struct dg_alloc_hdr);
        g_dg_heap_head->free = 1;
        g_dg_heap_head->next = (struct dg_alloc_hdr *)0;
        g_dg_heap_head->prev = (struct dg_alloc_hdr *)0;
        g_dg_heap_ready = 1;
    }

    cur = g_dg_heap_head;

    while (cur != (struct dg_alloc_hdr *)0) {
        if (cur->free != 0 && cur->size >= aligned) {
            size_t remain = cur->size - aligned;

            if (remain > sizeof(struct dg_alloc_hdr) + 16U) {
                struct dg_alloc_hdr *tail =
                    (struct dg_alloc_hdr *)(void *)((unsigned char *)(void *)(cur + 1) + aligned);
                tail->size = remain - sizeof(struct dg_alloc_hdr);
                tail->free = 1;
                tail->next = cur->next;
                tail->prev = cur;
                if (cur->next != (struct dg_alloc_hdr *)0) {
                    cur->next->prev = tail;
                }
                cur->next = tail;
                cur->size = aligned;
            }

            cur->free = 0;
            return (void *)(cur + 1);
        }

        cur = cur->next;
    }

    errno = ENOMEM;
    return (void *)0;
}

void free(void *ptr) {
    struct dg_alloc_hdr *hdr;

    if (ptr == (void *)0) {
        return;
    }

    hdr = ((struct dg_alloc_hdr *)ptr) - 1;
    hdr->free = 1;

    if (hdr->next != (struct dg_alloc_hdr *)0 && hdr->next->free != 0) {
        struct dg_alloc_hdr *next = hdr->next;
        hdr->size += sizeof(struct dg_alloc_hdr) + next->size;
        hdr->next = next->next;
        if (next->next != (struct dg_alloc_hdr *)0) {
            next->next->prev = hdr;
        }
    }

    if (hdr->prev != (struct dg_alloc_hdr *)0 && hdr->prev->free != 0) {
        struct dg_alloc_hdr *prev = hdr->prev;
        prev->size += sizeof(struct dg_alloc_hdr) + hdr->size;
        prev->next = hdr->next;
        if (hdr->next != (struct dg_alloc_hdr *)0) {
            hdr->next->prev = prev;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total;
    void *ptr;

    if (nmemb == 0U || size == 0U) {
        total = 1U;
    } else if (nmemb > ((size_t)-1) / size) {
        errno = ENOMEM;
        return (void *)0;
    } else {
        total = nmemb * size;
    }

    ptr = malloc(total);
    if (ptr != (void *)0) {
        memset(ptr, 0, total);
    }

    return ptr;
}

void *realloc(void *ptr, size_t size) {
    struct dg_alloc_hdr *old_hdr;
    size_t old_size;
    void *new_ptr;

    if (ptr == (void *)0) {
        return malloc(size);
    }

    if (size == 0U) {
        free(ptr);
        return (void *)0;
    }

    old_hdr = ((struct dg_alloc_hdr *)ptr) - 1;
    old_size = old_hdr->size;

    if (old_size >= size) {
        return ptr;
    }

    if (old_hdr->next != (struct dg_alloc_hdr *)0 && old_hdr->next->free != 0 &&
        old_size + sizeof(struct dg_alloc_hdr) + old_hdr->next->size >= size) {
        struct dg_alloc_hdr *next = old_hdr->next;
        old_hdr->size += sizeof(struct dg_alloc_hdr) + next->size;
        old_hdr->next = next->next;
        if (old_hdr->next != (struct dg_alloc_hdr *)0) {
            old_hdr->next->prev = old_hdr;
        }
        return ptr;
    }

    new_ptr = malloc(size);
    if (new_ptr == (void *)0) {
        return (void *)0;
    }

    memcpy(new_ptr, ptr, (old_size < size) ? old_size : size);
    free(ptr);
    return new_ptr;
}

double atof(const char *text) {
    const char *p = text;
    double value = 0.0;
    double frac = 0.1;
    int sign = 1;

    if (p == (const char *)0) {
        return 0.0;
    }

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }

    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        value = (value * 10.0) + (double)(*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            value += (double)(*p - '0') * frac;
            frac *= 0.1;
            p++;
        }
    }

    return (double)sign * value;
}

int strcasecmp(const char *left, const char *right) {
    unsigned char a;
    unsigned char b;

    if (left == right) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)dg_to_lower_ascii((unsigned char)*left);
        b = (unsigned char)dg_to_lower_ascii((unsigned char)*right);
        if (a != b) {
            return (a < b) ? -1 : 1;
        }
        left++;
        right++;
    }

    if (*left == *right) {
        return 0;
    }

    return ((unsigned char)*left < (unsigned char)*right) ? -1 : 1;
}

int strncasecmp(const char *left, const char *right, size_t size) {
    size_t i;

    if (size == 0U || left == right) {
        return 0;
    }

    for (i = 0U; i < size; i++) {
        unsigned char a = (unsigned char)dg_to_lower_ascii((unsigned char)left[i]);
        unsigned char b = (unsigned char)dg_to_lower_ascii((unsigned char)right[i]);

        if (a != b) {
            return (a < b) ? -1 : 1;
        }

        if (left[i] == '\0' || right[i] == '\0') {
            break;
        }
    }

    return 0;
}

char *strdup(const char *text) {
    size_t len;
    char *out;

    len = dg_strlen(text);
    out = (char *)malloc(len + 1U);
    if (out == (char *)0) {
        return (char *)0;
    }

    memcpy(out, text, len + 1U);
    return out;
}

char *getenv(const char *name) {
    static char line[DG_ENV_LINE_MAX];
    static char value[DG_ENV_LINE_MAX];
    u64 envc;
    u64 i;
    size_t nlen;

    if (name[0] == '\0') {
        return (char *)0;
    }

    nlen = dg_strlen(name);
    envc = cleonos_sys_proc_envc();

    for (i = 0ULL; i < envc; i++) {
        if (cleonos_sys_proc_env(i, line, (u64)sizeof(line)) == 0ULL) {
            continue;
        }

        if (strncmp(line, name, nlen) == 0 && line[nlen] == '=') {
            dg_copy(value, sizeof(value), line + nlen + 1U);
            return value;
        }
    }

    return (char *)0;
}

time_t time(time_t *out_time) {
    time_t value = (time_t)(cleonos_sys_timer_ticks() / 100ULL);

    if (out_time != (time_t *)0) {
        *out_time = value;
    }

    return value;
}

int access(const char *path, int mode) {
    char resolved[DG_PATH_MAX];
    (void)mode;

    dg_resolve_read_path(path, resolved, sizeof(resolved));
    if (dg_path_exists(resolved) == 0) {
        errno = ENOENT;
        return -1;
    }

    return 0;
}

int mkdir(const char *path, mode_t mode) {
    char resolved[DG_PATH_MAX];
    (void)mode;

    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    dg_resolve_write_path(path, resolved, sizeof(resolved));
    if (dg_is_temp_path(resolved) == 0) {
        errno = EACCES;
        return -1;
    }

    if (cleonos_sys_fs_mkdir(resolved) == 0ULL) {
        errno = EIO;
        return -1;
    }

    return 0;
}

int unlink(const char *path) {
    char resolved[DG_PATH_MAX];

    dg_resolve_write_path(path, resolved, sizeof(resolved));
    if (dg_is_temp_path(resolved) == 0) {
        errno = EACCES;
        return -1;
    }

    if (cleonos_sys_fs_remove(resolved) == 0ULL) {
        errno = EIO;
        return -1;
    }

    return 0;
}

int remove(const char *path) {
    return unlink(path);
}

int rename(const char *old_path, const char *new_path) {
    char src[DG_PATH_MAX];
    char dst[DG_PATH_MAX];
    u64 file_size;
    char *buf;
    u64 read_len;
    int rc = -1;

    if (old_path == (const char *)0 || new_path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    dg_resolve_read_path(old_path, src, sizeof(src));
    dg_resolve_write_path(new_path, dst, sizeof(dst));

    if (dg_is_temp_path(dst) == 0) {
        errno = EACCES;
        return -1;
    }

    file_size = cleonos_sys_fs_stat_size(src);
    if (file_size == (u64)-1) {
        errno = ENOENT;
        return -1;
    }

    buf = (char *)malloc((size_t)file_size + 1U);
    if (buf == (char *)0) {
        return -1;
    }

    read_len = cleonos_sys_fs_read(src, buf, file_size);
    if (read_len != file_size) {
        errno = EIO;
        goto done;
    }

    if (cleonos_sys_fs_write(dst, buf, read_len) != read_len) {
        errno = EIO;
        goto done;
    }

    if (cleonos_sys_fs_remove(src) == 0ULL) {
        errno = EIO;
        goto done;
    }

    rc = 0;
done:
    free(buf);
    return rc;
}

int open(const char *path, int flags, ...) {
    int slot;
    int fd;
    struct dg_mem_fd *file;
    int writable;
    char resolved[DG_PATH_MAX];

    writable = ((flags & O_ACCMODE) != O_RDONLY) ? 1 : 0;
    if (writable != 0) {
        dg_resolve_write_path(path, resolved, sizeof(resolved));
        if (dg_is_temp_path(resolved) == 0) {
            errno = EACCES;
            return -1;
        }
    } else {
        dg_resolve_read_path(path, resolved, sizeof(resolved));
    }

    slot = dg_fd_alloc_slot();
    if (slot < 0) {
        errno = EMFILE;
        return -1;
    }

    file = &g_dg_fds[slot];
    file->writable = writable;
    dg_copy(file->path, sizeof(file->path), resolved);

    if (writable == 0 || (flags & O_APPEND) != 0) {
        u64 size = cleonos_sys_fs_stat_size(resolved);

        if (size == (u64)-1) {
            if (writable == 0) {
                file->used = 0;
                errno = ENOENT;
                return -1;
            }
        } else if (size > 0ULL) {
            if (dg_fd_ensure_cap(file, (size_t)size) != 0) {
                free(file->data);
                file->used = 0;
                file->data = (unsigned char *)0;
                file->cap = 0U;
                return -1;
            }

            if (cleonos_sys_fs_read(resolved, (char *)file->data, size) != size) {
                free(file->data);
                file->used = 0;
                file->data = (unsigned char *)0;
                file->cap = 0U;
                errno = EIO;
                return -1;
            }

            file->size = (size_t)size;
            file->pos = ((flags & O_APPEND) != 0) ? file->size : 0U;
        }
    }

    if ((flags & O_TRUNC) != 0) {
        file->size = 0U;
        file->pos = 0U;
        file->dirty = 1;
    }

    fd = slot + 3;
    return fd;
}

int close(int fd) {
    int slot = dg_fd_slot_from_fd(fd);
    struct dg_mem_fd *file;

    if (fd >= 0 && fd <= 2) {
        return 0;
    }

    if (slot < 0) {
        errno = EBADF;
        return -1;
    }

    file = &g_dg_fds[slot];

    if (dg_fd_flush_slot(file) != 0) {
        return -1;
    }

    free(file->data);
    file->used = 0;
    file->writable = 0;
    file->dirty = 0;
    file->pos = 0U;
    file->size = 0U;
    file->cap = 0U;
    file->data = (unsigned char *)0;
    file->path[0] = '\0';
    return 0;
}

ssize_t read(int fd, void *out_buf, size_t size) {
    int slot;
    struct dg_mem_fd *file;
    size_t left;
    size_t n;

    if (out_buf == (void *)0) {
        errno = EINVAL;
        return -1;
    }

    if (size == 0U) {
        return 0;
    }

    if (fd == 0) {
        u64 got = cleonos_sys_fd_read(0ULL, out_buf, (u64)size);
        if (got == (u64)-1) {
            errno = EIO;
            return -1;
        }
        return (ssize_t)got;
    }

    slot = dg_fd_slot_from_fd(fd);
    if (slot < 0) {
        errno = EBADF;
        return -1;
    }

    file = &g_dg_fds[slot];
    left = (file->pos < file->size) ? (file->size - file->pos) : 0U;
    n = (size < left) ? size : left;

    if (n > 0U) {
        memcpy(out_buf, file->data + file->pos, n);
        file->pos += n;
    }

    return (ssize_t)n;
}

ssize_t write(int fd, const void *buf, size_t size) {
    int slot;
    struct dg_mem_fd *file;

    if (buf == (const void *)0) {
        errno = EINVAL;
        return -1;
    }

    if (size == 0U) {
        return 0;
    }

    if (fd == 1 || fd == 2) {
        u64 wrote = cleonos_sys_fd_write((u64)fd, buf, (u64)size);
        if (wrote == (u64)-1) {
            errno = EIO;
            return -1;
        }
        dg_serial_log_bytes(buf, (size_t)wrote);
        return (ssize_t)wrote;
    }

    slot = dg_fd_slot_from_fd(fd);
    if (slot < 0) {
        errno = EBADF;
        return -1;
    }

    file = &g_dg_fds[slot];
    if (file->writable == 0) {
        errno = EBADF;
        return -1;
    }

    if (dg_fd_ensure_cap(file, file->pos + size) != 0) {
        return -1;
    }

    memcpy(file->data + file->pos, buf, size);
    file->pos += size;
    if (file->pos > file->size) {
        file->size = file->pos;
    }
    file->dirty = 1;

    return (ssize_t)size;
}

off_t lseek(int fd, off_t offset, int whence) {
    int slot = dg_fd_slot_from_fd(fd);
    struct dg_mem_fd *file;
    long long base = 0LL;
    long long next;

    if (slot < 0) {
        errno = EBADF;
        return (off_t)-1;
    }

    file = &g_dg_fds[slot];

    if (whence == SEEK_SET) {
        base = 0LL;
    } else if (whence == SEEK_CUR) {
        base = (long long)file->pos;
    } else if (whence == SEEK_END) {
        base = (long long)file->size;
    } else {
        errno = EINVAL;
        return (off_t)-1;
    }

    next = base + (long long)offset;
    if (next < 0LL) {
        errno = EINVAL;
        return (off_t)-1;
    }

    file->pos = (size_t)next;
    return (off_t)file->pos;
}

int isatty(int fd) {
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

static struct dg_stream *dg_stream_ptr(FILE *stream) {
    struct dg_stream *s = (struct dg_stream *)(void *)stream;

    if (s == (struct dg_stream *)0 || s->magic != DG_STDIO_MAGIC) {
        return (struct dg_stream *)0;
    }

    return s;
}

FILE *dg_fopen(const char *path, const char *mode) {
    int flags = O_RDONLY;
    int fd;
    struct dg_stream *stream;

    if (path == (const char *)0 || mode == (const char *)0 || mode[0] == '\0') {
        errno = EINVAL;
        return (FILE *)0;
    }

    if (mode[0] == 'r') {
        flags = O_RDONLY;
    } else if (mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (mode[0] == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        errno = EINVAL;
        return (FILE *)0;
    }

    if (strchr(mode, '+') != (char *)0) {
        flags &= ~(O_RDONLY | O_WRONLY);
        flags |= O_RDWR;
    }

    fd = open(path, flags, 0644);
    if (fd < 0) {
        return (FILE *)0;
    }

    stream = (struct dg_stream *)malloc(sizeof(struct dg_stream));
    if (stream == (struct dg_stream *)0) {
        (void)close(fd);
        return (FILE *)0;
    }

    stream->magic = DG_STDIO_MAGIC;
    stream->fd = fd;
    stream->eof_flag = 0;
    stream->err_flag = 0;
    return (FILE *)(void *)stream;
}

int dg_fclose(FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);
    int rc;

    if (s == (struct dg_stream *)0) {
        errno = EINVAL;
        return EOF;
    }

    if (s == &g_dg_stdin_stream || s == &g_dg_stdout_stream || s == &g_dg_stderr_stream) {
        return 0;
    }

    rc = close(s->fd);
    free(s);
    return (rc == 0) ? 0 : EOF;
}

size_t dg_fread(void *out_buf, size_t size, size_t nmemb, FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);
    size_t want;
    ssize_t got;

    if (s == (struct dg_stream *)0 || out_buf == (void *)0 || size == 0U || nmemb == 0U) {
        return 0U;
    }

    if (nmemb > ((size_t)-1) / size) {
        s->err_flag = 1;
        errno = EINVAL;
        return 0U;
    }

    want = size * nmemb;
    got = read(s->fd, out_buf, want);
    if (got < 0) {
        s->err_flag = 1;
        return 0U;
    }

    if ((size_t)got < want) {
        s->eof_flag = 1;
    }

    return (size == 0U) ? 0U : ((size_t)got / size);
}

size_t dg_fwrite(const void *buf, size_t size, size_t nmemb, FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);
    size_t want;
    ssize_t wrote;

    if (s == (struct dg_stream *)0 || buf == (const void *)0 || size == 0U || nmemb == 0U) {
        return 0U;
    }

    if (nmemb > ((size_t)-1) / size) {
        s->err_flag = 1;
        errno = EINVAL;
        return 0U;
    }

    want = size * nmemb;
    wrote = write(s->fd, buf, want);
    if (wrote < 0) {
        s->err_flag = 1;
        return 0U;
    }

    return (size_t)wrote / size;
}

int dg_fseek(FILE *stream, long offset, int whence) {
    struct dg_stream *s = dg_stream_ptr(stream);

    if (s == (struct dg_stream *)0) {
        errno = EINVAL;
        return -1;
    }

    if (lseek(s->fd, (off_t)offset, whence) < 0) {
        s->err_flag = 1;
        return -1;
    }

    s->eof_flag = 0;
    return 0;
}

long dg_ftell(FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);
    off_t pos;

    if (s == (struct dg_stream *)0) {
        errno = EINVAL;
        return -1L;
    }

    pos = lseek(s->fd, 0, SEEK_CUR);
    if (pos < (off_t)0) {
        s->err_flag = 1;
        return -1L;
    }

    return (long)pos;
}

int dg_fflush(FILE *stream) {
    struct dg_stream *s;
    int slot;

    if (stream == (FILE *)0) {
        return 0;
    }

    s = dg_stream_ptr(stream);
    if (s == (struct dg_stream *)0) {
        errno = EINVAL;
        return EOF;
    }

    slot = dg_fd_slot_from_fd(s->fd);
    if (slot >= 0) {
        if (dg_fd_flush_slot(&g_dg_fds[slot]) != 0) {
            s->err_flag = 1;
            return EOF;
        }
    }

    return 0;
}

char *dg_fgets(char *out_text, int size, FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);
    int i = 0;
    char ch = '\0';

    if (s == (struct dg_stream *)0 || out_text == (char *)0 || size <= 1) {
        return (char *)0;
    }

    while (i + 1 < size) {
        ssize_t got = read(s->fd, &ch, 1U);

        if (got <= 0) {
            if (got < 0) {
                s->err_flag = 1;
            } else {
                s->eof_flag = 1;
            }
            break;
        }

        out_text[i++] = ch;
        if (ch == '\n') {
            break;
        }
    }

    if (i == 0) {
        return (char *)0;
    }

    out_text[i] = '\0';
    return out_text;
}

int dg_feof(FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);
    return (s == (struct dg_stream *)0) ? 1 : s->eof_flag;
}

int dg_fileno(FILE *stream) {
    struct dg_stream *s = dg_stream_ptr(stream);

    if (s == (struct dg_stream *)0) {
        errno = EINVAL;
        return -1;
    }

    return s->fd;
}

int dg_vfprintf(FILE *stream, const char *fmt, va_list args) {
    struct dg_stream *s = dg_stream_ptr(stream);
    int len;
    char local[512];
    va_list args_copy;

    if (s == (struct dg_stream *)0 || fmt == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    va_copy(args_copy, args);
    len = vsnprintf(local, sizeof(local), fmt, args_copy);
    va_end(args_copy);
    if (len < 0) {
        s->err_flag = 1;
        return -1;
    }

    if ((size_t)len < sizeof(local)) {
        if (write(s->fd, local, (size_t)len) < 0) {
            s->err_flag = 1;
            return -1;
        }
        return len;
    }

    {
        size_t cap = (size_t)len + 1U;
        char *dyn = (char *)malloc(cap);
        int written_len;

        if (dyn == (char *)0) {
            s->err_flag = 1;
            return -1;
        }

        va_copy(args_copy, args);
        written_len = vsnprintf(dyn, cap, fmt, args_copy);
        va_end(args_copy);

        if (written_len < 0 || write(s->fd, dyn, (size_t)written_len) < 0) {
            s->err_flag = 1;
            return -1;
        }

        return written_len;
    }
}

int dg_fprintf(FILE *stream, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = dg_vfprintf(stream, fmt, args);
    va_end(args);
    return rc;
}

void dg_perror(const char *text) {
    if (text != (const char *)0 && text[0] != '\0') {
        (void)dg_fprintf(stderr, "%s: errno=%d\n", text, errno);
    } else {
        (void)dg_fprintf(stderr, "errno=%d\n", errno);
    }
}

int dg_sscanf(const char *text, const char *fmt, ...) {
    const char *p = text;
    const char *f = fmt;
    int assigned = 0;
    va_list args;

    if (text == (const char *)0 || fmt == (const char *)0) {
        return 0;
    }

    va_start(args, fmt);

    while (*f != '\0') {
        if (*f != '%') {
            if (dg_is_space_char((unsigned char)*f) != 0) {
                while (dg_is_space_char((unsigned char)*p) != 0) {
                    p++;
                }
                f++;
                continue;
            }

            if (*p != *f) {
                break;
            }

            p++;
            f++;
            continue;
        }

        f++;
        if (*f == '\0') {
            break;
        }

        while (dg_is_space_char((unsigned char)*p) != 0) {
            p++;
        }

        if (*f == 'd' || *f == 'i') {
            int *out_i = va_arg(args, int *);
            char *endp = (char *)p;
            long v = strtol(p, &endp, (*f == 'i') ? 0 : 10);
            if (endp == p) {
                break;
            }
            if (out_i != (int *)0) {
                *out_i = (int)v;
            }
            p = endp;
            assigned++;
        } else if (*f == 'u' || *f == 'x' || *f == 'o') {
            unsigned int *out_u = va_arg(args, unsigned int *);
            int base = (*f == 'x') ? 16 : ((*f == 'o') ? 8 : 10);
            char *endp = (char *)p;
            unsigned long v = strtoul(p, &endp, base);
            if (endp == p) {
                break;
            }
            if (out_u != (unsigned int *)0) {
                *out_u = (unsigned int)v;
            }
            p = endp;
            assigned++;
        } else if (*f == 'f') {
            float *out_f = va_arg(args, float *);
            char *endp = (char *)p;
            double v = atof(p);
            while (*endp == '+' || *endp == '-' || *endp == '.' || (*endp >= '0' && *endp <= '9')) {
                endp++;
            }
            if (endp == p) {
                break;
            }
            if (out_f != (float *)0) {
                *out_f = (float)v;
            }
            p = endp;
            assigned++;
        } else if (*f == 's') {
            char *out_s = va_arg(args, char *);
            int copied = 0;
            if (out_s == (char *)0) {
                break;
            }
            while (*p != '\0' && dg_is_space_char((unsigned char)*p) == 0) {
                out_s[copied++] = *p++;
            }
            if (copied == 0) {
                break;
            }
            out_s[copied] = '\0';
            assigned++;
        } else if (*f == 'c') {
            char *out_c = va_arg(args, char *);
            if (*p == '\0' || out_c == (char *)0) {
                break;
            }
            *out_c = *p++;
            assigned++;
        } else {
            break;
        }

        f++;
    }

    va_end(args);
    return assigned;
}
