#include "cmd_runtime.h"
#include "minizip_cleonos_io.h"

#include <zip.h>

#define ZIP_BUF_SIZE 4096U
#define ZIP_MAX_INPUTS 16U

static char zip_buf[ZIP_BUF_SIZE];

static const char *zip_basename(const char *path) {
    const char *name = path;
    u64 i;

    if (path == (const char *)0) {
        return "";
    }

    for (i = 0ULL; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            name = path + i + 1ULL;
        }
    }

    return name;
}

static int zip_write_all_file(zipFile archive, const char *path) {
    u64 fd;

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        printf("zip: open failed: %s\n", path);
        return 0;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read(fd, zip_buf, (u64)sizeof(zip_buf));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            printf("zip: read failed: %s\n", path);
            return 0;
        }
        if (got == 0ULL) {
            break;
        }
        if (zipWriteInFileInZip(archive, zip_buf, (unsigned)got) != ZIP_OK) {
            (void)cleonos_sys_fd_close(fd);
            printf("zip: write zip entry failed: %s\n", path);
            return 0;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

static int zip_add_file(zipFile archive, const char *path, const char *entry_name) {
    zip_fileinfo info;
    int rc;

    memset(&info, 0, sizeof(info));
    info.tmz_date.tm_year = 2026;
    info.tmz_date.tm_mon = 0;
    info.tmz_date.tm_mday = 1;

    rc = zipOpenNewFileInZip(archive, entry_name, &info, NULL, 0U, NULL, 0U, NULL, Z_DEFLATED,
                             Z_DEFAULT_COMPRESSION);
    if (rc != ZIP_OK) {
        printf("zip: open entry failed: %s rc=%d\n", entry_name, rc);
        return 0;
    }

    if (zip_write_all_file(archive, path) == 0) {
        (void)zipCloseFileInZip(archive);
        return 0;
    }

    rc = zipCloseFileInZip(archive);
    if (rc != ZIP_OK) {
        printf("zip: close entry failed: %s rc=%d\n", entry_name, rc);
        return 0;
    }

    printf("  adding: %s\n", entry_name);
    return 1;
}

static int zip_run(const ush_state *sh, const char *arg) {
    zlib_filefunc64_def funcs;
    zipFile archive;
    char out_arg[USH_PATH_MAX];
    char out_path[USH_PATH_MAX];
    const char *rest = "";
    u64 added = 0ULL;

    if (sh == (const ush_state *)0 || arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, out_arg, (u64)sizeof(out_arg), &rest) == 0 ||
        ush_resolve_path(sh, out_arg, out_path, (u64)sizeof(out_path)) == 0) {
        return 0;
    }

    if (rest == (const char *)0 || rest[0] == '\0') {
        return 0;
    }

    cleonos_minizip_fill_filefunc64(&funcs);
    archive = zipOpen2_64(out_path, APPEND_STATUS_CREATE, NULL, &funcs);
    if (archive == (zipFile)0) {
        printf("zip: create failed: %s\n", out_path);
        return 0;
    }

    while (rest != (const char *)0 && rest[0] != '\0') {
        char file_arg[USH_PATH_MAX];
        char file_path[USH_PATH_MAX];

        if (ush_split_first_and_rest(rest, file_arg, (u64)sizeof(file_arg), &rest) == 0) {
            (void)zipClose(archive, NULL);
            return 0;
        }

        if (ush_resolve_path(sh, file_arg, file_path, (u64)sizeof(file_path)) == 0 ||
            cleonos_sys_fs_stat_type(file_path) != 1ULL) {
            printf("zip: input file not found: %s\n", file_arg);
            (void)zipClose(archive, NULL);
            return 0;
        }

        if (zip_add_file(archive, file_path, zip_basename(file_path)) == 0) {
            (void)zipClose(archive, NULL);
            return 0;
        }
        added++;
    }

    if (zipClose(archive, NULL) != ZIP_OK) {
        puts("zip: close archive failed");
        return 0;
    }

    printf("zip: wrote %llu entries to %s\n", (unsigned long long)added, out_path);
    return 1;
}

static void zip_help(void) {
    puts("usage:");
    puts("  zip <archive.zip> <file...>");
    puts("note: directories are not packed yet");
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    const char *arg = "";
    int has_context = 0;
    int ok;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "zip") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    ok = zip_run(&sh, arg);
    if (ok == 0) {
        zip_help();
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
