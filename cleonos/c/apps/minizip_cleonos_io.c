#include "minizip_cleonos_io.h"

#include <cleonos_syscall.h>
#include <stdlib.h>
#include <string.h>

typedef struct cleonos_mz_file {
    char path[192];
    unsigned char *data;
    ZPOS64_T size;
    ZPOS64_T capacity;
    ZPOS64_T pos;
    int writable;
    int error;
} cleonos_mz_file;

static int cleonos_mz_grow(cleonos_mz_file *file, ZPOS64_T needed) {
    unsigned char *next;
    ZPOS64_T new_capacity;

    if (file == (cleonos_mz_file *)0) {
        return 0;
    }
    if (needed <= file->capacity) {
        return 1;
    }

    new_capacity = (file->capacity != 0U) ? file->capacity : 4096U;
    while (new_capacity < needed) {
        ZPOS64_T doubled = new_capacity * 2U;
        if (doubled <= new_capacity) {
            return 0;
        }
        new_capacity = doubled;
    }

    next = (unsigned char *)malloc((size_t)new_capacity);
    if (next == (unsigned char *)0) {
        return 0;
    }

    if (file->data != (unsigned char *)0 && file->size != 0U) {
        memcpy(next, file->data, (size_t)file->size);
    }
    free(file->data);
    file->data = next;
    file->capacity = new_capacity;
    return 1;
}

static voidpf ZCALLBACK cleonos_mz_open64(voidpf opaque, const void *filename, int mode) {
    const char *path = (const char *)filename;
    cleonos_mz_file *file;
    u64 size;

    (void)opaque;
    if (path == (const char *)0 || path[0] == '\0') {
        return (voidpf)0;
    }

    file = (cleonos_mz_file *)malloc(sizeof(cleonos_mz_file));
    if (file == (cleonos_mz_file *)0) {
        return (voidpf)0;
    }
    memset(file, 0, sizeof(*file));
    strncpy(file->path, path, sizeof(file->path) - 1U);

    file->writable = ((mode & ZLIB_FILEFUNC_MODE_WRITE) != 0 || (mode & ZLIB_FILEFUNC_MODE_CREATE) != 0) ? 1 : 0;
    if (file->writable != 0 && (mode & ZLIB_FILEFUNC_MODE_CREATE) != 0) {
        return (voidpf)file;
    }

    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1) {
        free(file);
        return (voidpf)0;
    }

    if (size != 0ULL) {
        file->data = (unsigned char *)malloc((size_t)size);
        if (file->data == (unsigned char *)0) {
            free(file);
            return (voidpf)0;
        }
        if (cleonos_sys_fs_read(path, (char *)file->data, size) != size) {
            free(file->data);
            free(file);
            return (voidpf)0;
        }
    }

    file->size = (ZPOS64_T)size;
    file->capacity = (ZPOS64_T)size;
    if ((mode & ZLIB_FILEFUNC_MODE_CREATE) != 0) {
        file->size = 0U;
        file->pos = 0U;
    } else if ((mode & ZLIB_FILEFUNC_MODE_EXISTING) != 0) {
        file->pos = 0U;
    }

    return (voidpf)file;
}

static voidpf ZCALLBACK cleonos_mz_open32(voidpf opaque, const char *filename, int mode) {
    return cleonos_mz_open64(opaque, (const void *)filename, mode);
}

static uLong ZCALLBACK cleonos_mz_read(voidpf opaque, voidpf stream, void *buf, uLong size) {
    cleonos_mz_file *file = (cleonos_mz_file *)stream;
    ZPOS64_T available;
    ZPOS64_T n;

    (void)opaque;
    if (file == (cleonos_mz_file *)0 || buf == (void *)0 || size == 0U) {
        return 0U;
    }
    if (file->pos >= file->size) {
        return 0U;
    }

    available = file->size - file->pos;
    n = ((ZPOS64_T)size < available) ? (ZPOS64_T)size : available;
    memcpy(buf, file->data + (size_t)file->pos, (size_t)n);
    file->pos += n;
    return (uLong)n;
}

static uLong ZCALLBACK cleonos_mz_write(voidpf opaque, voidpf stream, const void *buf, uLong size) {
    cleonos_mz_file *file = (cleonos_mz_file *)stream;
    ZPOS64_T end_pos;

    (void)opaque;
    if (file == (cleonos_mz_file *)0 || buf == (const void *)0 || file->writable == 0) {
        return 0U;
    }

    end_pos = file->pos + (ZPOS64_T)size;
    if (end_pos < file->pos || cleonos_mz_grow(file, end_pos) == 0) {
        file->error = 1;
        return 0U;
    }

    memcpy(file->data + (size_t)file->pos, buf, (size_t)size);
    file->pos = end_pos;
    if (file->pos > file->size) {
        file->size = file->pos;
    }

    return size;
}

static ZPOS64_T ZCALLBACK cleonos_mz_tell64(voidpf opaque, voidpf stream) {
    cleonos_mz_file *file = (cleonos_mz_file *)stream;

    (void)opaque;
    return (file != (cleonos_mz_file *)0) ? file->pos : (ZPOS64_T)-1;
}

static long ZCALLBACK cleonos_mz_tell32(voidpf opaque, voidpf stream) {
    ZPOS64_T pos = cleonos_mz_tell64(opaque, stream);
    return (pos > (ZPOS64_T)0x7FFFFFFFUL) ? -1L : (long)pos;
}

static long ZCALLBACK cleonos_mz_seek64(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin) {
    cleonos_mz_file *file = (cleonos_mz_file *)stream;
    ZPOS64_T base;
    ZPOS64_T next;

    (void)opaque;
    if (file == (cleonos_mz_file *)0) {
        return -1L;
    }

    if (origin == ZLIB_FILEFUNC_SEEK_SET) {
        base = 0U;
    } else if (origin == ZLIB_FILEFUNC_SEEK_CUR) {
        base = file->pos;
    } else if (origin == ZLIB_FILEFUNC_SEEK_END) {
        base = file->size;
    } else {
        return -1L;
    }

    next = base + offset;
    if (next < base) {
        return -1L;
    }
    if (next > file->size && file->writable == 0) {
        return -1L;
    }
    if (next > file->capacity && cleonos_mz_grow(file, next) == 0) {
        file->error = 1;
        return -1L;
    }
    if (next > file->size) {
        memset(file->data + (size_t)file->size, 0, (size_t)(next - file->size));
        file->size = next;
    }

    file->pos = next;
    return 0L;
}

static long ZCALLBACK cleonos_mz_seek32(voidpf opaque, voidpf stream, uLong offset, int origin) {
    return cleonos_mz_seek64(opaque, stream, (ZPOS64_T)offset, origin);
}

static int ZCALLBACK cleonos_mz_close(voidpf opaque, voidpf stream) {
    cleonos_mz_file *file = (cleonos_mz_file *)stream;
    int ok = 1;

    (void)opaque;
    if (file == (cleonos_mz_file *)0) {
        return -1;
    }

    if (file->writable != 0 && file->error == 0) {
        u64 wrote = cleonos_sys_fs_write(file->path, (const char *)file->data, (u64)file->size);
        ok = (wrote == (u64)file->size) ? 1 : 0;
    }

    free(file->data);
    free(file);
    return (ok != 0) ? 0 : -1;
}

static int ZCALLBACK cleonos_mz_error(voidpf opaque, voidpf stream) {
    cleonos_mz_file *file = (cleonos_mz_file *)stream;

    (void)opaque;
    return (file != (cleonos_mz_file *)0 && file->error != 0) ? 1 : 0;
}

voidpf call_zopen64(const zlib_filefunc64_32_def *pfilefunc, const void *filename, int mode) {
    if (pfilefunc->zfile_func64.zopen64_file != NULL) {
        return (*(pfilefunc->zfile_func64.zopen64_file))(pfilefunc->zfile_func64.opaque, filename, mode);
    }
    return (*(pfilefunc->zopen32_file))(pfilefunc->zfile_func64.opaque, (const char *)filename, mode);
}

long call_zseek64(const zlib_filefunc64_32_def *pfilefunc, voidpf filestream, ZPOS64_T offset, int origin) {
    if (pfilefunc->zfile_func64.zseek64_file != NULL) {
        return (*(pfilefunc->zfile_func64.zseek64_file))(pfilefunc->zfile_func64.opaque, filestream, offset, origin);
    }
    if (offset > (ZPOS64_T)0xFFFFFFFFUL) {
        return -1L;
    }
    return (*(pfilefunc->zseek32_file))(pfilefunc->zfile_func64.opaque, filestream, (uLong)offset, origin);
}

ZPOS64_T call_ztell64(const zlib_filefunc64_32_def *pfilefunc, voidpf filestream) {
    if (pfilefunc->zfile_func64.ztell64_file != NULL) {
        return (*(pfilefunc->zfile_func64.ztell64_file))(pfilefunc->zfile_func64.opaque, filestream);
    }
    return (ZPOS64_T)(*(pfilefunc->ztell32_file))(pfilefunc->zfile_func64.opaque, filestream);
}

void fill_zlib_filefunc64_32_def_from_filefunc32(zlib_filefunc64_32_def *out, const zlib_filefunc_def *in) {
    out->zfile_func64.zopen64_file = NULL;
    out->zopen32_file = in->zopen_file;
    out->zfile_func64.zread_file = in->zread_file;
    out->zfile_func64.zwrite_file = in->zwrite_file;
    out->zfile_func64.ztell64_file = NULL;
    out->zfile_func64.zseek64_file = NULL;
    out->zfile_func64.zclose_file = in->zclose_file;
    out->zfile_func64.zerror_file = in->zerror_file;
    out->zfile_func64.opaque = in->opaque;
    out->zseek32_file = in->zseek_file;
    out->ztell32_file = in->ztell_file;
}

void cleonos_minizip_fill_filefunc64(zlib_filefunc64_def *funcs) {
    if (funcs == (zlib_filefunc64_def *)0) {
        return;
    }

    funcs->zopen64_file = cleonos_mz_open64;
    funcs->zread_file = cleonos_mz_read;
    funcs->zwrite_file = cleonos_mz_write;
    funcs->ztell64_file = cleonos_mz_tell64;
    funcs->zseek64_file = cleonos_mz_seek64;
    funcs->zclose_file = cleonos_mz_close;
    funcs->zerror_file = cleonos_mz_error;
    funcs->opaque = (voidpf)0;
}

void fill_fopen64_filefunc(zlib_filefunc64_def *funcs) {
    cleonos_minizip_fill_filefunc64(funcs);
}

void fill_fopen_filefunc(zlib_filefunc_def *funcs) {
    if (funcs == (zlib_filefunc_def *)0) {
        return;
    }

    funcs->zopen_file = cleonos_mz_open32;
    funcs->zread_file = cleonos_mz_read;
    funcs->zwrite_file = cleonos_mz_write;
    funcs->ztell_file = cleonos_mz_tell32;
    funcs->zseek_file = cleonos_mz_seek32;
    funcs->zclose_file = cleonos_mz_close;
    funcs->zerror_file = cleonos_mz_error;
    funcs->opaque = (voidpf)0;
}
