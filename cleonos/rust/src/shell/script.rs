use super::*;

pub(crate) fn run_script_file(sh: &mut ShellState, path: &[u8]) -> bool {
    let mut script = [0u8; SCRIPT_MAX + 1];
    let got = syscall(SYSCALL_FS_READ, path.as_ptr() as U64, script.as_mut_ptr() as U64, SCRIPT_MAX as U64);
    if got == 0 || got == !0 {
        return false;
    }
    let mut total = got as usize;
    if total > SCRIPT_MAX {
        total = SCRIPT_MAX;
    }
    script[total] = 0;
    let mut line = [0u8; LINE_MAX];
    let mut line_pos = 0usize;
    let mut i = 0usize;
    while i <= total {
        let ch = script[i];
        if ch == b'\r' {
            i += 1;
            continue;
        }
        if ch == b'\n' || ch == 0 {
            line[line_pos] = 0;
            execute_line(sh, &line);
            if sh.exit_requested {
                return true;
            }
            clear_buf(&mut line);
            line_pos = 0;
            i += 1;
            continue;
        }
        if line_pos + 1 < LINE_MAX {
            line[line_pos] = ch;
            line_pos += 1;
        }
        i += 1;
    }
    true
}

pub(crate) fn builtin_source(sh: &mut ShellState, arg: &[u8]) -> bool {
    let mut first = [0u8; PATH_MAX];
    let mut rest = [0u8; ARG_MAX];
    if !split_first_and_rest(arg, &mut first, &mut rest) {
        writeln(b"source: usage source <file>");
        return false;
    }
    if c_len(&rest) != 0 {
        writeln(b"source: too many arguments");
        return false;
    }
    let mut path = [0u8; PATH_MAX];
    if !resolve_path(&sh.cwd, &first, &mut path) {
        writeln(b"source: invalid path");
        return false;
    }
    if syscall(SYSCALL_FS_STAT_TYPE, path.as_ptr() as U64, 0, 0) != FS_TYPE_FILE {
        writeln(b"source: file not found");
        return false;
    }
    if !run_script_file(sh, &path) {
        writeln(b"source: failed");
        return false;
    }
    true
}
