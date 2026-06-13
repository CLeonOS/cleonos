use super::*;

pub(crate) fn execute_line(sh: &mut ShellState, line: &[u8]) {
    let mut line_buf = [0u8; LINE_MAX];
    let _ = copy_bytes(&mut line_buf, &line[..c_len(line)]);
    trim_ascii_in_place(&mut line_buf);
    if c_len(&line_buf) == 0 || line_buf[0] == b'#' {
        return;
    }
    let result = if pipeline_has_meta(&line_buf) {
        execute_pipeline(sh, &line_buf)
    } else {
        let mut cmd = [0u8; 32];
        let mut arg = [0u8; ARG_MAX];
        split_line(&line_buf, &mut cmd, &mut arg);
        trim_ascii_in_place(&mut arg);
        if c_len(&cmd) == 0 {
            return;
        }
        if let Some(success) = run_builtin(sh, &cmd, &arg) {
            ExecResult { known: true, success }
        } else {
            let result = exec_external(sh, &cmd, &arg);
            if !result.known {
                write_bytes(b"command not found (external ELF required): ");
                write_cstr(&cmd);
                write_bytes(b"\n");
            }
            result
        }
    };
    sh.cmd_total += 1;
    if result.success {
        sh.cmd_ok += 1;
    } else {
        sh.cmd_fail += 1;
    }
    if !result.known {
        sh.cmd_unknown += 1;
    }
}

pub(crate) fn argv_copy(argv: *mut *mut u8, index: isize, out: &mut [u8]) -> bool {
    clear_buf(out);
    if argv.is_null() || index < 0 {
        return false;
    }
    unsafe {
        let ptr_item = *argv.offset(index);
        copy_from_ptr(out, ptr_item as *const u8)
    }
}

pub(crate) fn run_startup_arguments(sh: &mut ShellState, argc: i32, argv: *mut *mut u8) {
    if argc <= 1 {
        return;
    }
    let mut first = [0u8; LINE_MAX];
    if !argv_copy(argv, 1, &mut first) {
        return;
    }
    if eq_cstr(&first, b"-c") {
        if argc > 2 {
            let mut line = [0u8; LINE_MAX];
            if argv_copy(argv, 2, &mut line) {
                execute_line(sh, &line);
            }
        } else {
            writeln(b"shell: -c requires a command");
        }
        return;
    }
    let mut i = 1i32;
    while i < argc {
        let mut script_arg = [0u8; PATH_MAX];
        if argv_copy(argv, i as isize, &mut script_arg) {
            let mut path = [0u8; PATH_MAX];
            if resolve_path(&sh.cwd, &script_arg, &mut path)
                && syscall(SYSCALL_FS_STAT_TYPE, path.as_ptr() as U64, 0, 0) == FS_TYPE_FILE
            {
                let _ = run_script_file(sh, &path);
            } else {
                write_bytes(b"shell: script not found: ");
                write_cstr(&script_arg);
                write_bytes(b"\n");
            }
            if sh.exit_requested {
                return;
            }
        }
        i += 1;
    }
}

pub(crate) fn shell_main_impl(argc: i32, argv: *mut *mut u8) -> i32 {
    let mut sh = ShellState {
        line: [0; LINE_MAX],
        line_len: 0,
        cursor: 0,
        rendered_len: 0,
        cwd: [0; PATH_MAX],
        username: [0; NAME_MAX],
        home: [0; HOME_MAX],
        role: USER_ROLE_ADMIN,
        disk_login_required: false,
        logged_in: false,
        history: [[0; LINE_MAX]; HISTORY_MAX],
        history_count: 0,
        history_nav: -1,
        nav_saved_line: [0; LINE_MAX],
        nav_saved_len: 0,
        nav_saved_cursor: 0,
        clipboard: [0; LINE_MAX],
        clipboard_len: 0,
        sel_start: 0,
        sel_end: 0,
        sel_active: false,
        sel_anchor: 0,
        sel_anchor_valid: false,
        cmd_total: 0,
        cmd_ok: 0,
        cmd_fail: 0,
        cmd_unknown: 0,
        exit_requested: false,
        exit_code: 0,
    };
    let _ = copy_bytes(&mut sh.cwd, b"/");
    let _ = copy_bytes(&mut sh.username, b"root");
    let _ = copy_bytes(&mut sh.home, b"/");

    writeln_i18n(
        b"\x1B[92m[USER][RUST-SHELL]\x1B[0m interactive framework online\0",
        "\x1B[92m[USER][RUST-SHELL]\x1B[0m 交互框架在线\0".as_bytes(),
    );
    let _ = syscall(SYSCALL_USER_SHELL_READY, 0, 0, 0);

    if !login_if_needed(&mut sh) {
        return 1;
    }

    let mut env_pwd = [0u8; PATH_MAX];
    if env_value(b"PWD", &mut env_pwd) && env_pwd[0] == b'/' {
        let _ = copy_bytes(&mut sh.cwd, &env_pwd[..c_len(&env_pwd)]);
    }

    history_load(&mut sh);
    run_startup_arguments(&mut sh, argc, argv);

    let mut line = [0u8; LINE_MAX];
    while !sh.exit_requested {
        read_interactive_line(&mut sh, &mut line);
        trim_ascii_in_place(&mut line);
        execute_line(&mut sh, &line);
    }
    (sh.exit_code & 0x7FFF_FFFF) as i32
}

#[no_mangle]
pub extern "C" fn cleonos_rust_shell_main(argc: i32, argv: *mut *mut u8, _envp: *mut *mut u8) -> i32 {
    shell_main_impl(argc, argv)
}
