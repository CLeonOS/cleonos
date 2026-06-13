use super::*;

pub(crate) fn history_cancel_nav(sh: &mut ShellState) {
    sh.history_nav = -1;
    sh.nav_saved_len = 0;
    sh.nav_saved_cursor = 0;
    clear_buf(&mut sh.nav_saved_line);
}

pub(crate) fn line_has_non_space(line: &[u8]) -> bool {
    let mut i = 0usize;
    while i < line.len() && line[i] != 0 {
        if !is_space(line[i]) {
            return true;
        }
        i += 1;
    }
    false
}

pub(crate) fn history_push_memory(sh: &mut ShellState, line: &[u8]) {
    if !line_has_non_space(line) {
        return;
    }
    if sh.history_count > 0 && eq_cstr(&sh.history[sh.history_count - 1], line) {
        return;
    }
    if sh.history_count < HISTORY_MAX {
        let idx = sh.history_count;
        let _ = copy_bytes(&mut sh.history[idx], &line[..c_len(line)]);
        sh.history_count += 1;
    } else {
        let mut i = 1usize;
        while i < HISTORY_MAX {
            sh.history[i - 1] = sh.history[i];
            i += 1;
        }
        let _ = copy_bytes(&mut sh.history[HISTORY_MAX - 1], &line[..c_len(line)]);
    }
}

pub(crate) fn history_storage_path() -> &'static [u8] {
    if syscall(SYSCALL_FS_STAT_TYPE, HISTORY_DIR.as_ptr() as U64, 0, 0) == FS_TYPE_DIR {
        HISTORY_PATH
    } else {
        HISTORY_FALLBACK_PATH
    }
}

pub(crate) fn history_load(sh: &mut ShellState) {
    let path = history_storage_path();
    if syscall(SYSCALL_FS_STAT_TYPE, path.as_ptr() as U64, 0, 0) != FS_TYPE_FILE {
        return;
    }
    let mut data = [0u8; HISTORY_DATA_MAX];
    let got = syscall(SYSCALL_FS_READ, path.as_ptr() as U64, data.as_mut_ptr() as U64, (HISTORY_DATA_MAX - 1) as U64);
    if got == 0 || got == !0 {
        return;
    }
    let mut total = got as usize;
    if total >= HISTORY_DATA_MAX {
        total = HISTORY_DATA_MAX - 1;
    }
    data[total] = 0;
    sh.history_count = 0;
    let mut start = 0usize;
    let mut pos = 0usize;
    while pos <= total {
        if data[pos] == b'\n' || data[pos] == 0 {
            let mut len = pos - start;
            if len > 0 && data[start + len - 1] == b'\r' {
                len -= 1;
            }
            if len > 0 {
                let mut line = [0u8; LINE_MAX];
                if len >= LINE_MAX {
                    len = LINE_MAX - 1;
                }
                let _ = copy_bytes(&mut line, &data[start..start + len]);
                history_push_memory(sh, &line);
            }
            start = pos + 1;
        }
        pos += 1;
    }
    history_cancel_nav(sh);
}

pub(crate) fn history_save(sh: &ShellState) {
    let mut data = [0u8; HISTORY_DATA_MAX];
    let mut len = 0usize;
    let mut i = 0usize;
    while i < sh.history_count {
        let line_len = c_len(&sh.history[i]);
        let mut j = 0usize;
        while j < line_len && len + 2 < data.len() {
            data[len] = sh.history[i][j];
            len += 1;
            j += 1;
        }
        if len + 1 < data.len() {
            data[len] = b'\n';
            len += 1;
        }
        i += 1;
    }
    let path = history_storage_path();
    let _ = syscall(SYSCALL_FS_WRITE, path.as_ptr() as U64, data.as_ptr() as U64, len as U64);
}

pub(crate) fn history_push(sh: &mut ShellState, line: &[u8]) {
    if !line_has_non_space(line) {
        history_cancel_nav(sh);
        return;
    }
    if sh.history_count > 0 && eq_cstr(&sh.history[sh.history_count - 1], line) {
        history_cancel_nav(sh);
        return;
    }
    history_push_memory(sh, line);
    history_save(sh);
    history_cancel_nav(sh);
}

pub(crate) fn load_line(sh: &mut ShellState, line: &[u8]) {
    let _ = copy_bytes(&mut sh.line, &line[..c_len(line)]);
    sh.line_len = c_len(&sh.line);
    sh.cursor = sh.line_len;
}

pub(crate) fn history_apply_current(sh: &mut ShellState) {
    if sh.history_nav >= 0 {
        let line = sh.history[sh.history_nav as usize];
        load_line(sh, &line);
    } else {
        sh.line = sh.nav_saved_line;
        sh.line_len = sh.nav_saved_len;
        sh.cursor = sh.nav_saved_cursor;
        if sh.line_len > LINE_MAX - 1 {
            sh.line_len = LINE_MAX - 1;
            sh.line[sh.line_len] = 0;
        }
        if sh.cursor > sh.line_len {
            sh.cursor = sh.line_len;
        }
    }
    render_line(sh);
}

pub(crate) fn history_up(sh: &mut ShellState) {
    if sh.history_count == 0 {
        return;
    }
    if sh.history_nav < 0 {
        sh.nav_saved_line = sh.line;
        sh.nav_saved_len = sh.line_len;
        sh.nav_saved_cursor = sh.cursor;
        sh.history_nav = sh.history_count as i64 - 1;
    } else if sh.history_nav > 0 {
        sh.history_nav -= 1;
    }
    history_apply_current(sh);
}

pub(crate) fn history_down(sh: &mut ShellState) {
    if sh.history_nav < 0 {
        return;
    }
    if (sh.history_nav as usize) + 1 < sh.history_count {
        sh.history_nav += 1;
    } else {
        sh.history_nav = -1;
    }
    history_apply_current(sh);
}

