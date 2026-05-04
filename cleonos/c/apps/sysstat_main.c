#include "cmd_runtime.h"

#define USH_SYSSTAT_MAX_IDS (CLEONOS_SYSCALL_LOCALE_SET + 1ULL)
#define USH_SYSSTAT_DEFAULT_TOP 12ULL

typedef struct ush_sysstat_entry {
    u64 id;
    u64 recent;
    u64 total;
    const char *name;
} ush_sysstat_entry;

static const char *ush_sysstat_name_for_id(u64 id) {
    switch (id) {
    case CLEONOS_SYSCALL_LOG_WRITE:
        return "LOG_WRITE";
    case CLEONOS_SYSCALL_TIMER_TICKS:
        return "TIMER_TICKS";
    case CLEONOS_SYSCALL_TASK_COUNT:
        return "TASK_COUNT";
    case CLEONOS_SYSCALL_CUR_TASK:
        return "CUR_TASK";
    case CLEONOS_SYSCALL_SERVICE_COUNT:
        return "SERVICE_COUNT";
    case CLEONOS_SYSCALL_SERVICE_READY_COUNT:
        return "SERVICE_READY";
    case CLEONOS_SYSCALL_CONTEXT_SWITCHES:
        return "CONTEXT_SWITCH";
    case CLEONOS_SYSCALL_KELF_COUNT:
        return "KELF_COUNT";
    case CLEONOS_SYSCALL_KELF_RUNS:
        return "KELF_RUNS";
    case CLEONOS_SYSCALL_FS_NODE_COUNT:
        return "FS_NODE_COUNT";
    case CLEONOS_SYSCALL_FS_CHILD_COUNT:
        return "FS_CHILD_COUNT";
    case CLEONOS_SYSCALL_FS_GET_CHILD_NAME:
        return "FS_CHILD_NAME";
    case CLEONOS_SYSCALL_FS_READ:
        return "FS_READ";
    case CLEONOS_SYSCALL_EXEC_PATH:
        return "EXEC_PATH";
    case CLEONOS_SYSCALL_EXEC_REQUESTS:
        return "EXEC_REQUESTS";
    case CLEONOS_SYSCALL_EXEC_SUCCESS:
        return "EXEC_SUCCESS";
    case CLEONOS_SYSCALL_USER_SHELL_READY:
        return "USER_SHELL_READY";
    case CLEONOS_SYSCALL_USER_EXEC_REQUESTED:
        return "USER_EXEC_REQ";
    case CLEONOS_SYSCALL_USER_LAUNCH_TRIES:
        return "USER_LAUNCH_TRY";
    case CLEONOS_SYSCALL_USER_LAUNCH_OK:
        return "USER_LAUNCH_OK";
    case CLEONOS_SYSCALL_USER_LAUNCH_FAIL:
        return "USER_LAUNCH_FAIL";
    case CLEONOS_SYSCALL_TTY_COUNT:
        return "TTY_COUNT";
    case CLEONOS_SYSCALL_TTY_ACTIVE:
        return "TTY_ACTIVE";
    case CLEONOS_SYSCALL_TTY_SWITCH:
        return "TTY_SWITCH";
    case CLEONOS_SYSCALL_TTY_WRITE:
        return "TTY_WRITE";
    case CLEONOS_SYSCALL_TTY_WRITE_CHAR:
        return "TTY_WRITE_CHAR";
    case CLEONOS_SYSCALL_KBD_GET_CHAR:
        return "KBD_GET_CHAR";
    case CLEONOS_SYSCALL_FS_STAT_TYPE:
        return "FS_STAT_TYPE";
    case CLEONOS_SYSCALL_FS_STAT_SIZE:
        return "FS_STAT_SIZE";
    case CLEONOS_SYSCALL_FS_MKDIR:
        return "FS_MKDIR";
    case CLEONOS_SYSCALL_FS_WRITE:
        return "FS_WRITE";
    case CLEONOS_SYSCALL_FS_APPEND:
        return "FS_APPEND";
    case CLEONOS_SYSCALL_FS_REMOVE:
        return "FS_REMOVE";
    case CLEONOS_SYSCALL_LOG_JOURNAL_COUNT:
        return "LOG_JCOUNT";
    case CLEONOS_SYSCALL_LOG_JOURNAL_READ:
        return "LOG_JREAD";
    case CLEONOS_SYSCALL_KBD_BUFFERED:
        return "KBD_BUFFERED";
    case CLEONOS_SYSCALL_KBD_PUSHED:
        return "KBD_PUSHED";
    case CLEONOS_SYSCALL_KBD_POPPED:
        return "KBD_POPPED";
    case CLEONOS_SYSCALL_KBD_DROPPED:
        return "KBD_DROPPED";
    case CLEONOS_SYSCALL_KBD_HOTKEY_SWITCHES:
        return "KBD_HOTKEYS";
    case CLEONOS_SYSCALL_GETPID:
        return "GETPID";
    case CLEONOS_SYSCALL_SPAWN_PATH:
        return "SPAWN_PATH";
    case CLEONOS_SYSCALL_WAITPID:
        return "WAITPID";
    case CLEONOS_SYSCALL_EXIT:
        return "EXIT";
    case CLEONOS_SYSCALL_SLEEP_TICKS:
        return "SLEEP_TICKS";
    case CLEONOS_SYSCALL_YIELD:
        return "YIELD";
    case CLEONOS_SYSCALL_SHUTDOWN:
        return "SHUTDOWN";
    case CLEONOS_SYSCALL_RESTART:
        return "RESTART";
    case CLEONOS_SYSCALL_AUDIO_AVAILABLE:
        return "AUDIO_AVAIL";
    case CLEONOS_SYSCALL_AUDIO_PLAY_TONE:
        return "AUDIO_TONE";
    case CLEONOS_SYSCALL_AUDIO_STOP:
        return "AUDIO_STOP";
    case CLEONOS_SYSCALL_EXEC_PATHV:
        return "EXEC_PATHV";
    case CLEONOS_SYSCALL_SPAWN_PATHV:
        return "SPAWN_PATHV";
    case CLEONOS_SYSCALL_PROC_ARGC:
        return "PROC_ARGC";
    case CLEONOS_SYSCALL_PROC_ARGV:
        return "PROC_ARGV";
    case CLEONOS_SYSCALL_PROC_ENVC:
        return "PROC_ENVC";
    case CLEONOS_SYSCALL_PROC_ENV:
        return "PROC_ENV";
    case CLEONOS_SYSCALL_PROC_LAST_SIGNAL:
        return "PROC_LAST_SIG";
    case CLEONOS_SYSCALL_PROC_FAULT_VECTOR:
        return "PROC_FAULT_VEC";
    case CLEONOS_SYSCALL_PROC_FAULT_ERROR:
        return "PROC_FAULT_ERR";
    case CLEONOS_SYSCALL_PROC_FAULT_RIP:
        return "PROC_FAULT_RIP";
    case CLEONOS_SYSCALL_PROC_COUNT:
        return "PROC_COUNT";
    case CLEONOS_SYSCALL_PROC_PID_AT:
        return "PROC_PID_AT";
    case CLEONOS_SYSCALL_PROC_SNAPSHOT:
        return "PROC_SNAPSHOT";
    case CLEONOS_SYSCALL_PROC_KILL:
        return "PROC_KILL";
    case CLEONOS_SYSCALL_KDBG_SYM:
        return "KDBG_SYM";
    case CLEONOS_SYSCALL_KDBG_BT:
        return "KDBG_BT";
    case CLEONOS_SYSCALL_KDBG_REGS:
        return "KDBG_REGS";
    case CLEONOS_SYSCALL_STATS_TOTAL:
        return "STATS_TOTAL";
    case CLEONOS_SYSCALL_STATS_ID_COUNT:
        return "STATS_ID_COUNT";
    case CLEONOS_SYSCALL_STATS_RECENT_WINDOW:
        return "STATS_RECENT_WIN";
    case CLEONOS_SYSCALL_STATS_RECENT_ID:
        return "STATS_RECENT_ID";
    case CLEONOS_SYSCALL_FD_OPEN:
        return "FD_OPEN";
    case CLEONOS_SYSCALL_FD_READ:
        return "FD_READ";
    case CLEONOS_SYSCALL_FD_WRITE:
        return "FD_WRITE";
    case CLEONOS_SYSCALL_FD_CLOSE:
        return "FD_CLOSE";
    case CLEONOS_SYSCALL_FD_DUP:
        return "FD_DUP";
    case CLEONOS_SYSCALL_DL_OPEN:
        return "DL_OPEN";
    case CLEONOS_SYSCALL_DL_CLOSE:
        return "DL_CLOSE";
    case CLEONOS_SYSCALL_DL_SYM:
        return "DL_SYM";
    case CLEONOS_SYSCALL_EXEC_PATHV_IO:
        return "EXEC_PATHV_IO";
    case CLEONOS_SYSCALL_FB_INFO:
        return "FB_INFO";
    case CLEONOS_SYSCALL_FB_BLIT:
        return "FB_BLIT";
    case CLEONOS_SYSCALL_FB_CLEAR:
        return "FB_CLEAR";
    case CLEONOS_SYSCALL_KERNEL_VERSION:
        return "KERNEL_VERSION";
    case CLEONOS_SYSCALL_DISK_PRESENT:
        return "DISK_PRESENT";
    case CLEONOS_SYSCALL_DISK_SIZE_BYTES:
        return "DISK_SIZE_BYTES";
    case CLEONOS_SYSCALL_DISK_SECTOR_COUNT:
        return "DISK_SECTOR_COUNT";
    case CLEONOS_SYSCALL_DISK_FORMATTED:
        return "DISK_FORMATTED";
    case CLEONOS_SYSCALL_DISK_FORMAT_FAT32:
        return "DISK_FORMAT_FAT32";
    case CLEONOS_SYSCALL_DISK_MOUNT:
        return "DISK_MOUNT";
    case CLEONOS_SYSCALL_DISK_MOUNTED:
        return "DISK_MOUNTED";
    case CLEONOS_SYSCALL_DISK_MOUNT_PATH:
        return "DISK_MOUNT_PATH";
    case CLEONOS_SYSCALL_DISK_READ_SECTOR:
        return "DISK_READ_SECTOR";
    case CLEONOS_SYSCALL_DISK_WRITE_SECTOR:
        return "DISK_WRITE_SECTOR";
    case CLEONOS_SYSCALL_NET_AVAILABLE:
        return "NET_AVAILABLE";
    case CLEONOS_SYSCALL_NET_IPV4_ADDR:
        return "NET_IPV4_ADDR";
    case CLEONOS_SYSCALL_NET_PING:
        return "NET_PING";
    case CLEONOS_SYSCALL_NET_UDP_SEND:
        return "NET_UDP_SEND";
    case CLEONOS_SYSCALL_NET_UDP_RECV:
        return "NET_UDP_RECV";
    case CLEONOS_SYSCALL_NET_NETMASK:
        return "NET_NETMASK";
    case CLEONOS_SYSCALL_NET_GATEWAY:
        return "NET_GATEWAY";
    case CLEONOS_SYSCALL_NET_DNS_SERVER:
        return "NET_DNS_SERVER";
    case CLEONOS_SYSCALL_NET_TCP_CONNECT:
        return "NET_TCP_CONNECT";
    case CLEONOS_SYSCALL_NET_TCP_SEND:
        return "NET_TCP_SEND";
    case CLEONOS_SYSCALL_NET_TCP_RECV:
        return "NET_TCP_RECV";
    case CLEONOS_SYSCALL_NET_TCP_CLOSE:
        return "NET_TCP_CLOSE";
    case CLEONOS_SYSCALL_MOUSE_STATE:
        return "MOUSE_STATE";
    case CLEONOS_SYSCALL_WM_CREATE:
        return "WM_CREATE";
    case CLEONOS_SYSCALL_WM_DESTROY:
        return "WM_DESTROY";
    case CLEONOS_SYSCALL_WM_PRESENT:
        return "WM_PRESENT";
    case CLEONOS_SYSCALL_WM_POLL_EVENT:
        return "WM_POLL_EVENT";
    case CLEONOS_SYSCALL_WM_MOVE:
        return "WM_MOVE";
    case CLEONOS_SYSCALL_WM_SET_FOCUS:
        return "WM_SET_FOCUS";
    case CLEONOS_SYSCALL_WM_SET_FLAGS:
        return "WM_SET_FLAGS";
    case CLEONOS_SYSCALL_WM_RESIZE:
        return "WM_RESIZE";
    case CLEONOS_SYSCALL_PTY_OPEN:
        return "PTY_OPEN";
    case CLEONOS_SYSCALL_WM_COUNT:
        return "WM_COUNT";
    case CLEONOS_SYSCALL_WM_ID_AT:
        return "WM_ID_AT";
    case CLEONOS_SYSCALL_WM_SNAPSHOT:
        return "WM_SNAPSHOT";
    case CLEONOS_SYSCALL_USER_HEAP_ALLOC:
        return "USER_HEAP_ALLOC";
    case CLEONOS_SYSCALL_DRIVER_COUNT:
        return "DRIVER_COUNT";
    case CLEONOS_SYSCALL_DRIVER_INFO:
        return "DRIVER_INFO";
    case CLEONOS_SYSCALL_DRIVER_LOAD:
        return "DRIVER_LOAD";
    case CLEONOS_SYSCALL_DRIVER_UNLOAD:
        return "DRIVER_UNLOAD";
    case CLEONOS_SYSCALL_DRIVER_RELOAD:
        return "DRIVER_RELOAD";
    case CLEONOS_SYSCALL_TIMER_HZ:
        return "TIMER_HZ";
    case CLEONOS_SYSCALL_TIME_MS:
        return "TIME_MS";
    case CLEONOS_SYSCALL_SLEEP_MS:
        return "SLEEP_MS";
    case CLEONOS_SYSCALL_NET_TCP_LAST_ERROR:
        return "NET_TCP_LAST_ERROR";
    case CLEONOS_SYSCALL_VM_ALLOC:
        return "VM_ALLOC";
    case CLEONOS_SYSCALL_VM_FREE:
        return "VM_FREE";
    case CLEONOS_SYSCALL_USER_CURRENT:
        return "USER_CURRENT";
    case CLEONOS_SYSCALL_USER_LOGIN:
        return "USER_LOGIN";
    case CLEONOS_SYSCALL_USER_LOGOUT:
        return "USER_LOGOUT";
    case CLEONOS_SYSCALL_USER_COUNT:
        return "USER_COUNT";
    case CLEONOS_SYSCALL_USER_AT:
        return "USER_AT";
    case CLEONOS_SYSCALL_USER_ADD:
        return "USER_ADD";
    case CLEONOS_SYSCALL_USER_PASSWD:
        return "USER_PASSWD";
    case CLEONOS_SYSCALL_USER_SET_ROLE:
        return "USER_SET_ROLE";
    case CLEONOS_SYSCALL_USER_REMOVE:
        return "USER_REMOVE";
    case CLEONOS_SYSCALL_USER_IS_ADMIN:
        return "USER_IS_ADMIN";
    case CLEONOS_SYSCALL_DISK_FSCK_FAT32:
        return "DISK_FSCK_FAT32";
    case CLEONOS_SYSCALL_SYSINFO:
        return "SYSINFO";
    case CLEONOS_SYSCALL_LOCALE_GET:
        return "LOCALE_GET";
    case CLEONOS_SYSCALL_LOCALE_SET:
        return "LOCALE_SET";
    default:
        return "UNKNOWN";
    }
}

static int ush_sysstat_next_token(const char **io_cursor, char *out, u64 out_size) {
    const char *p;
    u64 n = 0ULL;

    if (io_cursor == (const char **)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out[0] = '\0';
    p = *io_cursor;

    if (p == (const char *)0) {
        return 0;
    }

    while (*p != '\0' && ush_is_space(*p) != 0) {
        p++;
    }

    if (*p == '\0') {
        *io_cursor = p;
        return 0;
    }

    while (*p != '\0' && ush_is_space(*p) == 0) {
        if (n + 1ULL < out_size) {
            out[n++] = *p;
        }
        p++;
    }

    out[n] = '\0';
    *io_cursor = p;
    return 1;
}

static int ush_sysstat_parse_args(const char *arg, int *out_show_all, u64 *out_limit) {
    const char *cursor = arg;
    char token[USH_PATH_MAX];
    int show_all = 0;
    u64 limit = USH_SYSSTAT_DEFAULT_TOP;

    if (out_show_all == (int *)0 || out_limit == (u64 *)0) {
        return 0;
    }

    while (ush_sysstat_next_token(&cursor, token, (u64)sizeof(token)) != 0) {
        if (ush_streq(token, "-a") != 0 || ush_streq(token, "--all") != 0) {
            show_all = 1;
            continue;
        }

        if (ush_streq(token, "-n") != 0 || ush_streq(token, "--top") != 0) {
            if (ush_sysstat_next_token(&cursor, token, (u64)sizeof(token)) == 0 ||
                ush_parse_u64_dec(token, &limit) == 0) {
                return 0;
            }
            continue;
        }

        return 0;
    }

    if (limit == 0ULL) {
        limit = 1ULL;
    }

    *out_show_all = show_all;
    *out_limit = limit;
    return 1;
}

static void ush_sysstat_sort_recent(ush_sysstat_entry *entries, u64 count) {
    u64 i;

    if (entries == (ush_sysstat_entry *)0) {
        return;
    }

    for (i = 0ULL; i + 1ULL < count; i++) {
        u64 j;

        for (j = i + 1ULL; j < count; j++) {
            int swap = 0;

            if (entries[j].recent > entries[i].recent) {
                swap = 1;
            } else if (entries[j].recent == entries[i].recent && entries[j].total > entries[i].total) {
                swap = 1;
            } else if (entries[j].recent == entries[i].recent && entries[j].total == entries[i].total &&
                       entries[j].id < entries[i].id) {
                swap = 1;
            }

            if (swap != 0) {
                ush_sysstat_entry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
}

static int ush_cmd_sysstat(const char *arg) {
    int show_all = 0;
    u64 limit = USH_SYSSTAT_DEFAULT_TOP;
    ush_sysstat_entry entries[USH_SYSSTAT_MAX_IDS];
    u64 entry_count = 0ULL;
    u64 id;
    u64 total = cleonos_sys_stats_total();
    u64 recent_window = cleonos_sys_stats_recent_window();
    u64 to_show;

    if (ush_sysstat_parse_args(arg, &show_all, &limit) == 0) {
        ush_writeln("sysstat: usage sysstat [-a|--all] [-n N]");
        return 0;
    }

    ush_writeln("sysstat:");
    ush_print_kv_hex("  TIMER_TICKS", cleonos_sys_timer_ticks());
    ush_print_kv_hex("  TASK_COUNT", cleonos_sys_task_count());
    ush_print_kv_hex("  CURRENT_TASK", cleonos_syscall(CLEONOS_SYSCALL_CUR_TASK, 0ULL, 0ULL, 0ULL));
    ush_print_kv_hex("  CONTEXT_SWITCHES", cleonos_sys_context_switches());
    ush_print_kv_hex("  PROC_COUNT", cleonos_sys_proc_count());
    ush_print_kv_hex("  EXEC_REQUESTS", cleonos_sys_exec_request_count());
    ush_print_kv_hex("  EXEC_SUCCESS", cleonos_sys_exec_success_count());
    ush_print_kv_hex("  SYSCALL_TOTAL", total);
    ush_print_kv_hex("  SYSCALL_RECENT_WINDOW", recent_window);
    ush_writeln("");

    for (id = 0ULL; id < USH_SYSSTAT_MAX_IDS; id++) {
        u64 id_total = cleonos_sys_stats_id_count(id);
        u64 id_recent = cleonos_sys_stats_recent_id(id);

        if (show_all == 0 && id_total == 0ULL && id_recent == 0ULL) {
            continue;
        }

        entries[entry_count].id = id;
        entries[entry_count].recent = id_recent;
        entries[entry_count].total = id_total;
        entries[entry_count].name = ush_sysstat_name_for_id(id);
        entry_count++;
    }

    if (entry_count == 0ULL) {
        ush_writeln("(no syscall activity yet)");
        return 1;
    }

    if (show_all == 0) {
        ush_sysstat_sort_recent(entries, entry_count);
    }

    to_show = entry_count;
    if (show_all == 0 && to_show > limit) {
        to_show = limit;
    }

    for (id = 0ULL; id < to_show; id++) {
        ush_write("ID=");
        ush_write_hex_u64(entries[id].id);
        ush_write(" RECENT=");
        ush_write_hex_u64(entries[id].recent);
        ush_write(" TOTAL=");
        ush_write_hex_u64(entries[id].total);
        ush_write(" NAME=");
        ush_writeln(entries[id].name);
    }

    if (show_all == 0 && entry_count > to_show) {
        ush_write("... truncated, use ");
        ush_write("sysstat -a");
        ush_writeln(" to show all syscall IDs");
    }

    return 1;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "sysstat") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_sysstat(arg);

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
