#include "cmd_runtime.h"
#include "minizip_cleonos_io.h"

#include <unzip.h>

#define UNZIP_BUF_SIZE 4096U
#define UNZIP_NAME_MAX 160U

static char unzip_buf[UNZIP_BUF_SIZE];

typedef struct unzip_args {
    char zip_path[USH_PATH_MAX];
    char dst_path[USH_PATH_MAX];
    int extract;
} unzip_args;

static int unzip_is_safe_name(const char *name) {
    u64 i;

    if (name == (const char *)0 || name[0] == '\0' || name[0] == '/') {
        return 0;
    }

    for (i = 0ULL; name[i] != '\0'; i++) {
        if (name[i] == '\\') {
            return 0;
        }
        if (name[i] == '.' && (i == 0ULL || name[i - 1ULL] == '/') && name[i + 1ULL] == '.' &&
            (name[i + 2ULL] == '/' || name[i + 2ULL] == '\0')) {
            return 0;
        }
    }

    return 1;
}

static int unzip_mkdir_parents_for_file(const char *file_path) {
    char path[USH_PATH_MAX];
    u64 i;

    if (file_path == (const char *)0 || file_path[0] != '/') {
        return 0;
    }

    ush_copy(path, (u64)sizeof(path), file_path);
    for (i = 1ULL; path[i] != '\0'; i++) {
        if (path[i] != '/') {
            continue;
        }

        path[i] = '\0';
        if (path[0] != '\0' && cleonos_sys_fs_stat_type(path) != 2ULL &&
            cleonos_sys_fs_mkdir(path) == 0ULL) {
            return 0;
        }
        path[i] = '/';
    }

    return 1;
}

static int unzip_join(char *out, u64 out_size, const char *base, const char *name) {
    u64 base_len;
    u64 name_len;
    u64 need;

    if (out == (char *)0 || out_size == 0ULL || base == (const char *)0 || name == (const char *)0) {
        return 0;
    }

    base_len = ush_strlen(base);
    name_len = ush_strlen(name);
    need = base_len + ((base_len > 1ULL) ? 1ULL : 0ULL) + name_len + 1ULL;
    if (need > out_size) {
        return 0;
    }

    ush_copy(out, out_size, base);
    if (base_len > 1ULL && out[base_len - 1ULL] != '/') {
        out[base_len++] = '/';
        out[base_len] = '\0';
    }
    ush_copy(out + base_len, out_size - base_len, name);
    return 1;
}

static int unzip_write_all(u64 fd, const char *buf, u64 size) {
    u64 off = 0ULL;

    while (off < size) {
        u64 wrote = cleonos_sys_fd_write(fd, buf + off, size - off);
        if (wrote == 0ULL || wrote == (u64)-1) {
            return 0;
        }
        off += wrote;
    }

    return 1;
}

static int unzip_extract_current(unzFile zip, const char *dst_root, const char *name) {
    char out_path[USH_PATH_MAX];
    u64 fd;
    int rc;

    if (unzip_is_safe_name(name) == 0) {
        printf("unzip: skipped unsafe path: %s\n", name);
        return 1;
    }

    if (unzip_join(out_path, (u64)sizeof(out_path), dst_root, name) == 0) {
        printf("unzip: output path too long: %s\n", name);
        return 0;
    }

    if (name[ush_strlen(name) - 1ULL] == '/') {
        if (cleonos_sys_fs_stat_type(out_path) != 2ULL && cleonos_sys_fs_mkdir(out_path) == 0ULL) {
            printf("unzip: mkdir failed: %s\n", out_path);
            return 0;
        }
        return 1;
    }

    if (unzip_mkdir_parents_for_file(out_path) == 0) {
        printf("unzip: mkdir parents failed: %s\n", out_path);
        return 0;
    }

    rc = unzOpenCurrentFile(zip);
    if (rc != UNZ_OK) {
        printf("unzip: open entry failed: %s rc=%d\n", name, rc);
        return 0;
    }

    fd = cleonos_sys_fd_open(out_path, CLEONOS_O_WRONLY | CLEONOS_O_CREAT | CLEONOS_O_TRUNC, 0ULL);
    if (fd == (u64)-1) {
        (void)unzCloseCurrentFile(zip);
        printf("unzip: create failed: %s\n", out_path);
        return 0;
    }

    for (;;) {
        int got = unzReadCurrentFile(zip, unzip_buf, (unsigned)sizeof(unzip_buf));
        if (got < 0) {
            (void)cleonos_sys_fd_close(fd);
            (void)unzCloseCurrentFile(zip);
            printf("unzip: read entry failed: %s rc=%d\n", name, got);
            return 0;
        }
        if (got == 0) {
            break;
        }
        if (unzip_write_all(fd, unzip_buf, (u64)got) == 0) {
            (void)cleonos_sys_fd_close(fd);
            (void)unzCloseCurrentFile(zip);
            printf("unzip: write failed: %s\n", out_path);
            return 0;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    rc = unzCloseCurrentFile(zip);
    if (rc != UNZ_OK) {
        printf("unzip: crc failed: %s rc=%d\n", name, rc);
        return 0;
    }

    printf("  inflating: %s\n", out_path);
    return 1;
}

static int unzip_run(const unzip_args *args) {
    zlib_filefunc64_def funcs;
    unzFile zip;
    unz_global_info64 info;
    u64 index;
    u64 extracted = 0ULL;

    if (args == (const unzip_args *)0 || args->zip_path[0] == '\0') {
        return 0;
    }

    cleonos_minizip_fill_filefunc64(&funcs);
    zip = unzOpen2_64(args->zip_path, &funcs);
    if (zip == (unzFile)0) {
        printf("unzip: cannot open zip: %s\n", args->zip_path);
        return 0;
    }

    if (unzGetGlobalInfo64(zip, &info) != UNZ_OK) {
        (void)unzClose(zip);
        puts("unzip: invalid zip central directory");
        return 0;
    }

    printf("Archive: %s\n", args->zip_path);
    printf("Entries: %llu\n", (unsigned long long)info.number_entry);

    if (unzGoToFirstFile(zip) != UNZ_OK) {
        (void)unzClose(zip);
        return (info.number_entry == 0U) ? 1 : 0;
    }

    for (index = 0ULL; index < (u64)info.number_entry; index++) {
        char name[UNZIP_NAME_MAX];
        unz_file_info64 file_info;
        int rc;

        rc = unzGetCurrentFileInfo64(zip, &file_info, name, (uLong)sizeof(name), NULL, 0U, NULL, 0U);
        if (rc != UNZ_OK) {
            printf("unzip: entry info failed rc=%d\n", rc);
            (void)unzClose(zip);
            return 0;
        }
        name[sizeof(name) - 1U] = '\0';

        if (args->extract == 0) {
            printf("%10llu  %s\n", (unsigned long long)file_info.uncompressed_size, name);
        } else if (unzip_extract_current(zip, args->dst_path, name) == 0) {
            (void)unzClose(zip);
            return 0;
        } else {
            extracted++;
        }

        if (index + 1ULL < (u64)info.number_entry) {
            rc = unzGoToNextFile(zip);
            if (rc != UNZ_OK) {
                printf("unzip: next entry failed rc=%d\n", rc);
                (void)unzClose(zip);
                return 0;
            }
        }
    }

    (void)unzClose(zip);
    if (args->extract != 0) {
        printf("unzip: extracted %llu entries\n", (unsigned long long)extracted);
    }
    return 1;
}

static int unzip_parse(const ush_state *sh, const char *arg, unzip_args *out) {
    char first[USH_PATH_MAX];
    char second[USH_PATH_MAX];
    const char *rest = "";

    if (sh == (const ush_state *)0 || out == (unzip_args *)0) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    ush_copy(out->dst_path, (u64)sizeof(out->dst_path), sh->cwd);

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "-l") != 0 || ush_streq(first, "list") != 0) {
        out->extract = 0;
        if (rest == (const char *)0 || rest[0] == '\0') {
            return 0;
        }
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest) == 0) {
            return 0;
        }
        return ush_resolve_path(sh, second, out->zip_path, (u64)sizeof(out->zip_path));
    }

    if (ush_streq(first, "-x") != 0 || ush_streq(first, "extract") != 0) {
        out->extract = 1;
        if (rest == (const char *)0 || rest[0] == '\0') {
            return 0;
        }
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest) == 0) {
            return 0;
        }
        if (ush_resolve_path(sh, second, out->zip_path, (u64)sizeof(out->zip_path)) == 0) {
            return 0;
        }
        if (rest != (const char *)0 && rest[0] != '\0') {
            char dst_arg[USH_PATH_MAX];
            if (ush_split_first_and_rest(rest, dst_arg, (u64)sizeof(dst_arg), &rest) == 0) {
                return 0;
            }
            if (ush_resolve_path(sh, dst_arg, out->dst_path, (u64)sizeof(out->dst_path)) == 0) {
                return 0;
            }
        }
        return 1;
    }

    out->extract = 0;
    return ush_resolve_path(sh, first, out->zip_path, (u64)sizeof(out->zip_path));
}

static void unzip_help(void) {
    puts("usage:");
    puts("  unzip <archive.zip>");
    puts("  unzip -l <archive.zip>");
    puts("  unzip -x <archive.zip> [dest]");
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    unzip_args args;
    const char *arg = "";
    int has_context = 0;
    int ok = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "unzip") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    if (unzip_parse(&sh, arg, &args) == 0) {
        unzip_help();
    } else {
        ok = unzip_run(&args);
    }

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

    return (ok != 0) ? 0 : 1;
}
