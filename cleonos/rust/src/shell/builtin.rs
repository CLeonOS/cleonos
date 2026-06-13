use super::*;

pub(crate) fn builtin_cd(sh: &mut ShellState, arg: &[u8]) -> bool {
    let mut target = [0u8; PATH_MAX];
    let mut path = [0u8; PATH_MAX];
    if c_len(arg) == 0 {
        let _ = copy_bytes(&mut target, b"/");
    } else if eq_cstr(arg, b"~") {
        let _ = copy_bytes(&mut target, &sh.home[..c_len(&sh.home)]);
    } else {
        let _ = copy_bytes(&mut target, &arg[..c_len(arg)]);
    }
    if c_len(&target) == 0 {
        let _ = copy_bytes(&mut target, b"/");
    }
    if !resolve_path(&sh.cwd, &target, &mut path) {
        writeln_i18n(b"cd: invalid path\0", "cd: 无效路径\0".as_bytes());
        return false;
    }
    if syscall(SYSCALL_FS_STAT_TYPE, path.as_ptr() as U64, 0, 0) != 2 {
        writeln_i18n(b"cd: directory not found\0", "cd: 目录不存在\0".as_bytes());
        return false;
    }
    let _ = copy_bytes(&mut sh.cwd, &path[..c_len(&path)]);
    true
}

pub(crate) fn print_help() {
    writeln(b"Rust User Shell commands:");
    writeln(b"  shell: help, cd [dir], pwd, clear/cls, source <file> or . <file>, exit [code]");
    writeln(b"  exec: exec|run <path|name> [args...], spawn, bg, wait, fg, kill, jobs, ps, procstat, top");
    writeln(b"  files: ls/dir, cat, grep, head, tail, wc, cut, uniq, sort, mkdir, touch, write, append, cp, mv, rm");
    writeln(b"  system: uname/sysinfo, bootargs, locale, tty, resolution, dmesg, kbdstat, memstat, fsstat");
    writeln(b"  system: taskstat, userstat, shstat, stats, sysstat, kdbg, drvctl, systemctl, control");
    writeln(b"  users: whoami, passwd, logout, users, useradd, userdel, usermod");
    writeln(b"  disk: diskinfo, mkfsfat32, mount, partctl, fsckfat32, install2disk");
    writeln(b"  network: ping, ifconfig, nslookup, httpget, wget, httpd, webconsole, rsh, rshd");
    writeln(b"  apps: fastfetch, leonfetch, lua, bdt, tcc, hbos, doom, calc, browser, vim, imgview");
    writeln(b"  apps: bmpview, qrcode, calendar, contacts, note, todo, wavplay, zip, unzip, benchmark");
    writeln(b"  tests/libs: args, ansi, ansitest, hello, fdtest, dltest, devtest, libctest, libdemo");
    writeln(b"  tests/libs: pngtest, zlibtest, sqlitetest, stbtest, timertest, vmtest, ttftest");
    writeln(b"  tui/gui: uwm, terminal, taskmgr, file_explorer, pkg_gui, termbox2, termboxdemo, tui, tuitest");
    writeln(b"  stardust: stardust_helloworld, stardust_layout, uwm_uilib");
    writeln(b"  inputm: pinyin, romaji, emoji, symbols, chinese");
    writeln(b"  power: sleep, spin, yield, shutdown/poweroff, restart/reboot");
    writeln(b"pipeline/redirection: cmd1 | cmd2 | cmd3 > out.txt");
    writeln(b"redirection append:   cmd >> out.txt");
    writeln(b"edit keys: Left/Right, Home/End, Up/Down history, Tab completion, Ctrl+R search");
    writeln(b"           Ctrl+A/E home/end, Ctrl+U/K/W cut, Ctrl+L clear, Ctrl+D eof/delete");
    writeln(b"           selection/copy/paste keys and trailing \\ continues the command");
}

pub(crate) fn builtin_shstat(sh: &ShellState) {
    writeln(b"Rust User Shell status:");
    print_kv_dec(b"  commands", sh.cmd_total);
    print_kv_dec(b"  ok", sh.cmd_ok);
    print_kv_dec(b"  failed", sh.cmd_fail);
    print_kv_dec(b"  unknown", sh.cmd_unknown);
    print_kv_dec(b"  history", sh.history_count as U64);
    print_kv_hex(b"  last exit code", sh.exit_code);
    write_bytes(b"  cwd: ");
    write_cstr(&sh.cwd);
    write_bytes(b"\n  user: ");
    write_cstr(&sh.username);
    write_bytes(b"\n");
}

pub(crate) fn run_builtin(sh: &mut ShellState, cmd: &[u8], arg: &[u8]) -> Option<bool> {
    if eq_cstr(cmd, b"exit\0") {
        let mut code = 0u64;
        if c_len(arg) != 0 && !parse_u64_dec(arg, &mut code) {
            writeln(b"exit: numeric code required");
            return Some(false);
        }
        sh.exit_requested = true;
        sh.exit_code = code;
        return Some(true);
    }
    if eq_cstr(cmd, b"clear\0") || eq_cstr(cmd, b"cls\0") {
        write_bytes(b"\x1B[2J\x1B[3J\x1B[H");
        return Some(true);
    }
    if eq_cstr(cmd, b"pwd\0") {
        write_cstr(&sh.cwd);
        write_bytes(b"\n");
        return Some(true);
    }
    if eq_cstr(cmd, b"cd\0") {
        return Some(builtin_cd(sh, arg));
    }
    if eq_cstr(cmd, b"help\0") {
        print_help();
        return Some(true);
    }
    if eq_cstr(cmd, b"logout\0") {
        if !sh.disk_login_required {
            writeln(b"logout: login is disabled in ISO temporary mode");
            return Some(false);
        }
        let _ = syscall(SYSCALL_USER_LOGOUT, 0, 0, 0);
        sh.logged_in = false;
        writeln(b"logout: logged out");
        return Some(login_if_needed(sh));
    }
    if eq_cstr(cmd, b"source\0") || eq_cstr(cmd, b".\0") {
        return Some(builtin_source(sh, arg));
    }
    if eq_cstr(cmd, b"shstat\0") {
        builtin_shstat(sh);
        return Some(true);
    }
    None
}

