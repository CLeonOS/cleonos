#include "cmd_runtime.h"
#include <stdio.h>
#include <string.h>

#define RSHD_DEFAULT_PORT 2222ULL
#define RSHD_POLL_BUDGET 200000000ULL
#define RSHD_LINE_MAX 192ULL
#define RSHD_CHUNK 1024ULL
#define RSHD_OUT_LIMIT (512ULL * 1024ULL)
#define RSHD_IDLE_SLEEP_MS 10ULL

static int rshd_send_all(const char *data, u64 len) {
    u64 sent_total = 0ULL;

    while (sent_total < len) {
        cleonos_net_tcp_send_req req;
        u64 sent;

        req.payload_ptr = (u64)(usize)(data + sent_total);
        req.payload_len = len - sent_total;
        req.poll_budget = RSHD_POLL_BUDGET;
        sent = cleonos_sys_net_tcp_send(&req);
        if (sent == 0ULL) {
            return 0;
        }
        sent_total += sent;
    }

    return 1;
}

static int rshd_send_text(const char *text) {
    return rshd_send_all(text, (text != (const char *)0) ? (u64)strlen(text) : 0ULL);
}

static int rshd_send_prompt(const ush_state *sh) {
    char prompt[USH_PATH_MAX + 32U];
    int len = snprintf(prompt, sizeof(prompt), "cleonos:%s$ ", (sh != (const ush_state *)0) ? sh->cwd : "/");
    if (len <= 0 || (u64)len >= (u64)sizeof(prompt)) {
        return rshd_send_text("cleonos$ ");
    }
    return rshd_send_all(prompt, (u64)len);
}

static int rshd_recv_line(char *out_line, u64 out_size) {
    u64 pos = 0ULL;

    if (out_line == (char *)0 || out_size == 0ULL) {
        return 0;
    }
    out_line[0] = '\0';

    for (;;) {
        char ch;
        cleonos_net_tcp_recv_req req;
        u64 got;

        req.out_payload_ptr = (u64)(usize)&ch;
        req.payload_capacity = 1ULL;
        req.poll_budget = RSHD_POLL_BUDGET;
        got = cleonos_sys_net_tcp_recv(&req);
        if (got == 0ULL) {
            (void)cleonos_sys_sleep_ms(RSHD_IDLE_SLEEP_MS);
            continue;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            out_line[pos] = '\0';
            return 1;
        }
        if (pos + 1ULL < out_size) {
            out_line[pos++] = ch;
        }
    }
}

static int rshd_write_command_ctx(const char *cmd, const char *arg, const char *cwd) {
    ush_cmd_ctx ctx;

    ush_zero(&ctx, (u64)sizeof(ctx));
    if (cmd != (const char *)0) {
        ush_copy(ctx.cmd, (u64)sizeof(ctx.cmd), cmd);
    }
    if (arg != (const char *)0) {
        ush_copy(ctx.arg, (u64)sizeof(ctx.arg), arg);
    }
    if (cwd != (const char *)0) {
        ush_copy(ctx.cwd, (u64)sizeof(ctx.cwd), cwd);
    }

    return (cleonos_sys_fs_write(USH_CMD_CTX_PATH, (const char *)&ctx, (u64)sizeof(ctx)) != 0ULL) ? 1 : 0;
}

static int rshd_read_command_ret(ush_cmd_ret *out_ret) {
    u64 got;

    if (out_ret == (ush_cmd_ret *)0) {
        return 0;
    }

    ush_zero(out_ret, (u64)sizeof(*out_ret));
    got = cleonos_sys_fs_read(USH_CMD_RET_PATH, (char *)out_ret, (u64)sizeof(*out_ret));
    return (got == (u64)sizeof(*out_ret)) ? 1 : 0;
}

static int rshd_apply_ret(ush_state *sh, const ush_cmd_ret *ret) {
    if (sh == (ush_state *)0 || ret == (const ush_cmd_ret *)0) {
        return 0;
    }
    if ((ret->flags & USH_CMD_RET_FLAG_CWD) != 0ULL && ret->cwd[0] == '/') {
        ush_copy(sh->cwd, (u64)sizeof(sh->cwd), ret->cwd);
    }
    if ((ret->flags & USH_CMD_RET_FLAG_EXIT) != 0ULL) {
        sh->exit_requested = 1;
        sh->exit_code = ret->exit_code;
    }
    return 1;
}

static void rshd_format_status(char *out, u64 out_size, u64 status) {
    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }

    if (status == (u64)-1) {
        (void)snprintf(out, (unsigned long)out_size, "exec: request failed\n");
    } else if ((status & (1ULL << 63)) != 0ULL) {
        (void)snprintf(out, (unsigned long)out_size,
                       "exec: terminated by signal\n  SIGNAL: 0x%llX\n  VECTOR: 0x%llX\n  ERROR: 0x%llX\n",
                       (unsigned long long)(status & 0xFFULL), (unsigned long long)((status >> 8ULL) & 0xFFULL),
                       (unsigned long long)((status >> 16ULL) & 0xFFFFULL));
    } else if (status != 0ULL) {
        (void)snprintf(out, (unsigned long)out_size, "exec: returned non-zero status\n  STATUS: 0x%llX\n",
                       (unsigned long long)status);
    } else {
        out[0] = '\0';
    }
}

static int rshd_send_file_and_remove(const char *path) {
    char buf[RSHD_CHUNK];
    u64 fd;
    u64 sent_bytes = 0ULL;

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        (void)cleonos_sys_fs_remove(path);
        return 1;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            (void)cleonos_sys_fs_remove(path);
            return 0;
        }
        if (got == 0ULL) {
            break;
        }
        sent_bytes += got;
        if (sent_bytes > RSHD_OUT_LIMIT) {
            (void)rshd_send_text("\n[rshd] output truncated\n");
            break;
        }
        if (rshd_send_all(buf, got) == 0) {
            (void)cleonos_sys_fd_close(fd);
            (void)cleonos_sys_fs_remove(path);
            return 0;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    (void)cleonos_sys_fs_remove(path);
    return 1;
}

static int rshd_exec_external(ush_state *sh, const char *cmd, const char *arg) {
    char path[USH_PATH_MAX];
    char out_path[USH_PATH_MAX];
    char env_line[(USH_PATH_MAX * 2ULL) + USH_CMD_MAX + 96ULL];
    char status_text[160];
    u64 out_fd;
    u64 in_fd;
    u64 status;
    ush_cmd_ret ret;

    if (sh == (ush_state *)0 || cmd == (const char *)0 || cmd[0] == '\0') {
        return 0;
    }
    if (ush_resolve_exec_path(sh, cmd, path, (u64)sizeof(path)) == 0 || cleonos_sys_fs_stat_type(path) != 1ULL) {
        (void)rshd_send_text("command not found\n");
        return 0;
    }

    (void)snprintf(out_path, sizeof(out_path), "/temp/rshd-%llu.out", (unsigned long long)cleonos_sys_getpid());
    (void)cleonos_sys_fs_remove(out_path);
    out_fd = cleonos_sys_fd_open(out_path, CLEONOS_O_WRONLY | CLEONOS_O_CREAT | CLEONOS_O_TRUNC | CLEONOS_O_APPEND,
                                 0ULL);
    if (out_fd == (u64)-1) {
        (void)rshd_send_text("rshd: output capture open failed\n");
        return 0;
    }
    in_fd = cleonos_sys_fd_open("/dev/null", CLEONOS_O_RDONLY, 0ULL);

    (void)cleonos_sys_fs_remove(USH_CMD_CTX_PATH);
    (void)cleonos_sys_fs_remove(USH_CMD_RET_PATH);
    if (rshd_write_command_ctx(cmd, arg, sh->cwd) == 0) {
        (void)cleonos_sys_fd_close(out_fd);
        if (in_fd != (u64)-1) {
            (void)cleonos_sys_fd_close(in_fd);
        }
        (void)cleonos_sys_fs_remove(out_path);
        (void)rshd_send_text("rshd: command context write failed\n");
        return 0;
    }

    (void)snprintf(env_line, sizeof(env_line), "PWD=%s;CMD=%s;LAUNCHER=/shell/rshd.elf;USH_REMOTE=1", sh->cwd,
                   cmd);
    status = cleonos_sys_exec_pathv_io(path, arg, env_line, (in_fd == (u64)-1) ? CLEONOS_FD_INHERIT : in_fd, out_fd,
                                       out_fd);

    (void)cleonos_sys_fd_close(out_fd);
    if (in_fd != (u64)-1) {
        (void)cleonos_sys_fd_close(in_fd);
    }

    if (rshd_read_command_ret(&ret) != 0) {
        (void)rshd_apply_ret(sh, &ret);
    }

    (void)rshd_send_file_and_remove(out_path);
    status_text[0] = '\0';
    rshd_format_status(status_text, (u64)sizeof(status_text), status);
    if (status_text[0] != '\0') {
        (void)rshd_send_text(status_text);
    }

    (void)cleonos_sys_fs_remove(USH_CMD_CTX_PATH);
    (void)cleonos_sys_fs_remove(USH_CMD_RET_PATH);
    return (status == 0ULL) ? 1 : 0;
}

static int rshd_handle_builtin(ush_state *sh, const char *cmd, const char *arg, int *out_done) {
    char path[USH_PATH_MAX];

    if (out_done != (int *)0) {
        *out_done = 1;
    }

    if (ush_streq(cmd, "exit") != 0 || ush_streq(cmd, "quit") != 0) {
        sh->exit_requested = 1;
        return rshd_send_text("bye\n");
    }
    if (ush_streq(cmd, "pwd") != 0) {
        (void)rshd_send_text(sh->cwd);
        return rshd_send_text("\n");
    }
    if (ush_streq(cmd, "cd") != 0) {
        if (ush_resolve_path(sh, (arg != (const char *)0 && arg[0] != '\0') ? arg : "/", path, (u64)sizeof(path)) == 0 ||
            cleonos_sys_fs_stat_type(path) != 2ULL) {
            return rshd_send_text("cd: no such directory\n");
        }
        ush_copy(sh->cwd, (u64)sizeof(sh->cwd), path);
        return 1;
    }
    if (ush_streq(cmd, "help") != 0) {
        return rshd_send_text("remote commands: help, pwd, cd <dir>, exit, or any /shell/*.elf command\n");
    }

    if (out_done != (int *)0) {
        *out_done = 0;
    }
    return 0;
}

static int rshd_login(ush_state *sh) {
    char name[CLEONOS_USER_NAME_MAX];
    char password[96];
    cleonos_user_info info;
    int attempt;

    if (sh == (ush_state *)0) {
        return 0;
    }

    for (attempt = 0; attempt < 3; attempt++) {
        (void)rshd_send_text("login: ");
        if (rshd_recv_line(name, (u64)sizeof(name)) == 0) {
            return 0;
        }
        ush_trim_line(name);

        (void)rshd_send_text("password: ");
        if (rshd_recv_line(password, (u64)sizeof(password)) == 0) {
            return 0;
        }
        ush_trim_line(password);

        ush_zero(&info, (u64)sizeof(info));
        if (cleonos_sys_user_login(name, password, &info) != 0ULL && info.logged_in != 0ULL) {
            if (info.home[0] == '/' && cleonos_sys_fs_stat_type(info.home) == 2ULL) {
                ush_copy(sh->cwd, (u64)sizeof(sh->cwd), info.home);
            }
            (void)rshd_send_text("login ok\n");
            return 1;
        }

        (void)rshd_send_text("login failed\n");
    }

    return 0;
}

static int rshd_handle_client(void) {
    ush_state sh;
    char line[RSHD_LINE_MAX];

    ush_init_state(&sh);
    (void)rshd_send_text("CLeonOS remote shell 0.1\nwarning: password and command data are sent as plaintext\n");
    if (rshd_login(&sh) == 0) {
        return 0;
    }

    while (sh.exit_requested == 0) {
        char cmd[USH_CMD_MAX];
        char arg[USH_ARG_MAX];
        int done = 0;

        if (rshd_send_prompt(&sh) == 0) {
            return 0;
        }
        if (rshd_recv_line(line, (u64)sizeof(line)) == 0) {
            return 1;
        }
        ush_trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        ush_parse_line(line, cmd, (u64)sizeof(cmd), arg, (u64)sizeof(arg));
        ush_trim_line(arg);
        if (rshd_handle_builtin(&sh, cmd, arg, &done) == 0 && done != 0) {
            return 0;
        }
        if (done == 0) {
            (void)rshd_exec_external(&sh, cmd, arg);
        }
    }

    return 1;
}

static u64 rshd_parse_port(const char *text) {
    u64 value = 0ULL;
    u64 i;

    if (text == (const char *)0 || text[0] == '\0') {
        return 0ULL;
    }
    for (i = 0ULL; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0ULL;
        }
        value = value * 10ULL + (u64)(text[i] - '0');
        if (value > 65535ULL) {
            return 0ULL;
        }
    }
    return value;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    u64 port = RSHD_DEFAULT_PORT;
    u64 served = 0ULL;
    u64 max_clients = 0ULL;
    int i;

    (void)envp;
    for (i = 1; i < argc; i++) {
        if (argv[i] != (char *)0 && strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            u64 parsed = rshd_parse_port(argv[++i]);
            if (parsed == 0ULL) {
                puts("rshd: invalid port");
                return 1;
            }
            port = parsed;
        } else if (argv[i] != (char *)0 && strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_clients = rshd_parse_port(argv[++i]);
        } else {
            puts("usage: rshd [-p port] [-n max_clients]");
            return 1;
        }
    }

    (void)printf("rshd: listening on port %llu\n", (unsigned long long)port);
    for (;;) {
        cleonos_net_tcp_listen_req listen_req;
        cleonos_net_tcp_accept_req accept_req;

        listen_req.port = port;
        if (cleonos_sys_net_tcp_listen(&listen_req) == 0ULL) {
            puts("rshd: listen failed");
            return 1;
        }

        accept_req.poll_budget = RSHD_POLL_BUDGET;
        if (cleonos_sys_net_tcp_accept(&accept_req) == 0ULL) {
            continue;
        }

        (void)rshd_handle_client();
        (void)cleonos_sys_net_tcp_close(RSHD_POLL_BUDGET);
        served++;
        if (max_clients != 0ULL && served >= max_clients) {
            break;
        }
    }

    return 0;
}
