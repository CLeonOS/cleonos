use super::*;
use core::ptr;

pub(crate) fn command_ret_reset() {
    let _ = syscall(SYSCALL_FS_REMOVE, CMD_RET_PATH.as_ptr() as U64, 0, 0);
}

pub(crate) fn command_ret_read(out: &mut CmdRet) -> bool {
    unsafe {
        ptr::write_bytes(out as *mut CmdRet as *mut u8, 0, core::mem::size_of::<CmdRet>());
    }
    let got = syscall(
        SYSCALL_FS_READ,
        CMD_RET_PATH.as_ptr() as U64,
        out as *mut _ as U64,
        core::mem::size_of::<CmdRet>() as U64,
    );
    got == core::mem::size_of::<CmdRet>() as U64
}

pub(crate) fn command_ctx_write(cmd: &[u8], arg: &[u8], cwd: &[u8]) -> bool {
    let mut ctx = CmdCtx {
        cmd: [0; 32],
        arg: [0; ARG_MAX],
        cwd: [0; PATH_MAX],
    };
    let _ = copy_bytes(&mut ctx.cmd, &cmd[..c_len(cmd)]);
    let _ = copy_bytes(&mut ctx.arg, &arg[..c_len(arg)]);
    let _ = copy_bytes(&mut ctx.cwd, &cwd[..c_len(cwd)]);
    syscall(
        SYSCALL_FS_WRITE,
        CMD_CTX_PATH.as_ptr() as U64,
        &ctx as *const _ as U64,
        core::mem::size_of::<CmdCtx>() as U64,
    ) != 0
}

pub(crate) fn signal_name(signal: U64) -> &'static [u8] {
    if signal == 9 {
        b"KILL"
    } else if signal == 15 {
        b"TERM"
    } else if signal == 18 {
        b"CONT"
    } else if signal == 19 {
        b"STOP"
    } else {
        b"UNKNOWN"
    }
}

pub(crate) fn print_exit_status(label: &[u8], status: U64) {
    write_cstr(label);
    write_bytes(b": ");
    if (status & (1u64 << 63)) != 0 {
        let signal = status & 0xFF;
        let vector = (status >> 8) & 0xFF;
        let error = (status >> 16) & 0xFFFF;
        write_bytes(b"terminated by signal ");
        write_cstr(signal_name(signal));
        write_bytes(b" (");
        write_u64_dec(signal);
        write_bytes(b"), exception vector ");
        write_u64_dec(vector);
        write_bytes(b", error ");
        write_u64_dec(error);
        write_bytes(b"\n");
    } else {
        write_bytes(b"exit code ");
        write_u64_dec(status);
        write_bytes(b"\n");
    }
}

pub(crate) fn starts_with_raw(text: &[u8], prefix: &[u8]) -> bool {
    let len = c_len(text);
    len >= prefix.len() && text[..prefix.len()] == *prefix
}

pub(crate) fn alias_command(cmd: &[u8], out: &mut [u8]) {
    if eq_cstr(cmd, b"dir") {
        let _ = copy_bytes(out, b"ls");
    } else if eq_cstr(cmd, b"run") {
        let _ = copy_bytes(out, b"exec");
    } else if eq_cstr(cmd, b"poweroff") {
        let _ = copy_bytes(out, b"shutdown");
    } else if eq_cstr(cmd, b"reboot") {
        let _ = copy_bytes(out, b"restart");
    } else if eq_cstr(cmd, b"cls") {
        let _ = copy_bytes(out, b"clear");
    } else if eq_cstr(cmd, b"color") {
        let _ = copy_bytes(out, b"ansi");
    } else {
        let _ = copy_bytes(out, &cmd[..c_len(cmd)]);
    }
}

pub(crate) fn build_env(sh: &ShellState, cmd: &[u8], stdin_fd: U64, env: &mut [u8]) -> bool {
    clear_buf(env);
    append_bytes(env, b"PWD=")
        && append_cstr(env, &sh.cwd)
        && append_bytes(env, b";CMD=")
        && append_cstr(env, cmd)
        && append_bytes(env, b";LAUNCHER=/shell/apps/shell.elf;USER=")
        && append_cstr(env, &sh.username)
        && append_bytes(env, b";HOME=")
        && append_cstr(env, &sh.home)
        && append_bytes(env, b";ROLE=")
        && append_bytes(env, if sh.role == USER_ROLE_ADMIN { b"admin" } else { b"user" })
        && (stdin_fd == FD_INHERIT || append_bytes(env, b";USH_STDIN_MODE=PIPE"))
}

pub(crate) fn apply_command_ret(sh: &mut ShellState, ret: &CmdRet) {
    if (ret.flags & CMD_RET_FLAG_CWD) != 0 && ret.cwd[0] == b'/' {
        let _ = copy_bytes(&mut sh.cwd, &ret.cwd[..c_len(&ret.cwd)]);
    }
    if (ret.flags & CMD_RET_FLAG_EXIT) != 0 {
        sh.exit_requested = true;
        sh.exit_code = ret.exit_code;
    }
}

pub(crate) fn sync_user_state_after_external(sh: &mut ShellState, stdin_fd: U64, stdout_fd: U64, stderr_fd: U64) {
    if stdin_fd != FD_INHERIT || stdout_fd != FD_INHERIT || stderr_fd != FD_INHERIT {
        return;
    }
    let mut info = UserInfo {
        uid: 0,
        role: 0,
        logged_in: 0,
        disk_login_required: 0,
        name: [0; NAME_MAX],
        home: [0; HOME_MAX],
    };
    if syscall(SYSCALL_USER_CURRENT, &mut info as *mut _ as U64, 0, 0) != 0 {
        sh.disk_login_required = info.disk_login_required != 0;
        if info.logged_in != 0 {
            let _ = copy_bytes(&mut sh.username, &info.name[..c_len(&info.name)]);
            let _ = copy_bytes(&mut sh.home, &info.home[..c_len(&info.home)]);
            sh.role = info.role;
            sh.logged_in = true;
            return;
        }
    }
    clear_buf(&mut sh.username);
    clear_buf(&mut sh.home);
    sh.role = 0;
    sh.logged_in = false;
    let _ = login_if_needed(sh);
}

pub(crate) fn exec_external_with_fds(
    sh: &mut ShellState,
    cmd: &[u8],
    arg: &[u8],
    stdin_fd: U64,
    stdout_fd: U64,
    stderr_fd: U64,
) -> ExecResult {
    let mut canonical = [0u8; 32];
    let mut effective_arg = [0u8; ARG_MAX];
    let mut path = [0u8; PATH_MAX];
    let mut env = [0u8; ENV_MAX];
    alias_command(cmd, &mut canonical);
    if eq_cstr(&canonical, b"sysinfo") {
        let _ = copy_bytes(&mut canonical, b"uname");
        if c_len(arg) == 0 {
            let _ = copy_bytes(&mut effective_arg, b"--sysinfo");
        } else {
            let _ = copy_bytes(&mut effective_arg, b"--sysinfo ");
            let _ = append_cstr(&mut effective_arg, arg);
        }
    } else {
        let _ = copy_bytes(&mut effective_arg, &arg[..c_len(arg)]);
    }
    if !resolve_exec_path(&sh.cwd, &canonical, &mut path) {
        return ExecResult { known: false, success: false };
    }
    if starts_with_raw(&path, b"/system") {
        writeln_i18n(
            b"exec: /system is reserved for system files\0",
            "exec: /system 是系统文件保留路径\0".as_bytes(),
        );
        return ExecResult { known: true, success: false };
    }
    if syscall(SYSCALL_FS_STAT_TYPE, path.as_ptr() as U64, 0, 0) != 1 {
        return ExecResult { known: false, success: false };
    }
    command_ret_reset();
    if !command_ctx_write(&canonical, &effective_arg, &sh.cwd) {
        writeln(b"exec: command context write failed\0");
        return ExecResult { known: true, success: false };
    }
    if !build_env(sh, &canonical, stdin_fd, &mut env) {
        writeln(b"exec: env too long\0");
        let _ = syscall(SYSCALL_FS_REMOVE, CMD_CTX_PATH.as_ptr() as U64, 0, 0);
        return ExecResult { known: true, success: false };
    }
    let req = ExecPathvIoReq {
        env_line_ptr: env.as_ptr() as U64,
        stdin_fd,
        stdout_fd,
        stderr_fd,
    };
    let status = syscall(
        SYSCALL_EXEC_PATHV_IO,
        path.as_ptr() as U64,
        effective_arg.as_ptr() as U64,
        &req as *const _ as U64,
    );
    if status == !0 {
        writeln(b"exec: request failed\0");
        let _ = syscall(SYSCALL_FS_REMOVE, CMD_CTX_PATH.as_ptr() as U64, 0, 0);
        return ExecResult { known: true, success: false };
    }
    let mut ret = CmdRet {
        flags: 0,
        exit_code: 0,
        cwd: [0; PATH_MAX],
    };
    if command_ret_read(&mut ret) {
        apply_command_ret(sh, &ret);
    }
    sync_user_state_after_external(sh, stdin_fd, stdout_fd, stderr_fd);
    let _ = syscall(SYSCALL_FS_REMOVE, CMD_CTX_PATH.as_ptr() as U64, 0, 0);
    let _ = syscall(SYSCALL_FS_REMOVE, CMD_RET_PATH.as_ptr() as U64, 0, 0);
    if status != 0 {
        if (status & (1u64 << 63)) != 0 {
            print_exit_status(b"exec", status);
        } else {
            print_exit_status(b"exec returned non-zero status", status);
        }
        return ExecResult { known: true, success: false };
    }
    ExecResult { known: true, success: true }
}

pub(crate) fn exec_external(sh: &mut ShellState, cmd: &[u8], arg: &[u8]) -> ExecResult {
    exec_external_with_fds(sh, cmd, arg, FD_INHERIT, FD_INHERIT, FD_INHERIT)
}
