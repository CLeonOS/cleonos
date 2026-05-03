#include "shell_internal.h"

static void ush_apply_user_record(ush_state *sh, const cleonos_user_record *record) {
    if (sh == (ush_state *)0 || record == (const cleonos_user_record *)0) {
        return;
    }

    ush_copy(sh->username, (u64)sizeof(sh->username), record->name);
    ush_copy(sh->home, (u64)sizeof(sh->home), record->home);
    sh->user_role = record->role;
    sh->logged_in = 1;

    if (record->home[0] == '/' && cleonos_sys_fs_stat_type(record->home) == 2ULL) {
        ush_copy(sh->cwd, (u64)sizeof(sh->cwd), record->home);
    } else {
        ush_copy(sh->cwd, (u64)sizeof(sh->cwd), "/");
    }
}

static void ush_apply_kernel_user_info(ush_state *sh, const cleonos_user_info *info) {
    cleonos_user_record record;

    if (sh == (ush_state *)0 || info == (const cleonos_user_info *)0) {
        return;
    }

    (void)memset(&record, 0, sizeof(record));
    ush_copy(record.name, (u64)sizeof(record.name), info->name);
    record.role = info->role;
    ush_copy(record.home, (u64)sizeof(record.home), info->home);
    ush_apply_user_record(sh, &record);
}

int ush_login_if_needed(ush_state *sh) {
    cleonos_user_info info;

    if (sh == (ush_state *)0) {
        return 0;
    }

    (void)memset(&info, 0, sizeof(info));
    if (cleonos_sys_user_current(&info) != 0ULL) {
        sh->disk_login_required = (info.disk_login_required != 0ULL) ? 1 : 0;
        if (info.logged_in != 0ULL) {
            ush_apply_kernel_user_info(sh, &info);
            return 1;
        }
    } else {
        sh->disk_login_required = cleonos_user_is_disk_boot();
    }

    if (sh->disk_login_required == 0) {
        (void)cleonos_user_session_clear();
        (void)cleonos_sys_user_login("root", "", (cleonos_user_info *)0);
        ush_copy(sh->username, (u64)sizeof(sh->username), "root");
        ush_copy(sh->home, (u64)sizeof(sh->home), "/");
        sh->user_role = CLEONOS_USER_ROLE_ADMIN;
        sh->logged_in = 1;
        return 1;
    }

    ush_writeln("CLeonOS disk login");

    for (;;) {
        char name[CLEONOS_USER_NAME_MAX];
        char password[96];
        cleonos_user_info login_info;

        ush_read_plain_line("login: ", name, (u64)sizeof(name));
        ush_trim_line(name);

        if (name[0] == '\0') {
            continue;
        }

        ush_read_secret_line("password: ", password, (u64)sizeof(password));

        (void)memset(&login_info, 0, sizeof(login_info));
        if (cleonos_sys_user_login(name, password, &login_info) != 0ULL) {
            ush_apply_kernel_user_info(sh, &login_info);
            ush_write("login: welcome ");
            ush_writeln(login_info.name);
            return 1;
        }

        ush_writeln("login: invalid username or password");
    }
}
