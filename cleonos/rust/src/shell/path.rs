use super::*;

pub(crate) fn path_push(path: &mut [u8], io_len: &mut usize, component: &[u8]) -> bool {
    if component.is_empty() {
        return true;
    }
    if *io_len == 1 {
        if *io_len + component.len() >= path.len() {
            return false;
        }
        let mut i = 0usize;
        while i < component.len() {
            path[1 + i] = component[i];
            i += 1;
        }
        *io_len = 1 + component.len();
        path[*io_len] = 0;
        return true;
    }
    if *io_len + 1 + component.len() >= path.len() {
        return false;
    }
    path[*io_len] = b'/';
    let mut i = 0usize;
    while i < component.len() {
        path[*io_len + 1 + i] = component[i];
        i += 1;
    }
    *io_len += 1 + component.len();
    path[*io_len] = 0;
    true
}

pub(crate) fn path_pop(path: &mut [u8], io_len: &mut usize) {
    if *io_len <= 1 {
        path[0] = b'/';
        path[1] = 0;
        *io_len = 1;
        return;
    }
    while *io_len > 1 && path[*io_len - 1] != b'/' {
        *io_len -= 1;
    }
    if *io_len > 1 {
        *io_len -= 1;
    }
    path[*io_len] = 0;
}

pub(crate) fn path_parse_into(src: &[u8], out: &mut [u8], io_len: &mut usize) -> bool {
    let mut i = if src.first().copied() == Some(b'/') { 1 } else { 0 };
    let src_len = c_len(src);
    while i < src_len {
        while i < src_len && src[i] == b'/' {
            i += 1;
        }
        if i >= src_len {
            break;
        }
        let start = i;
        while i < src_len && src[i] != b'/' {
            i += 1;
        }
        let comp = &src[start..i];
        if comp == b"." {
            continue;
        }
        if comp == b".." {
            path_pop(out, io_len);
            continue;
        }
        if !path_push(out, io_len, comp) {
            return false;
        }
    }
    true
}

pub(crate) fn resolve_path(cwd: &[u8], arg: &[u8], out: &mut [u8]) -> bool {
    if out.len() < 2 {
        return false;
    }
    clear_buf(out);
    out[0] = b'/';
    out[1] = 0;
    let mut len = 1usize;
    if c_len(arg) == 0 {
        return path_parse_into(cwd, out, &mut len);
    }
    if arg[0] != b'/' && !path_parse_into(cwd, out, &mut len) {
        return false;
    }
    path_parse_into(arg, out, &mut len)
}

pub(crate) fn resolve_exec_path(cwd: &[u8], cmd: &[u8], out: &mut [u8]) -> bool {
    clear_buf(out);
    if c_len(cmd) == 0 {
        return false;
    }
    if cmd[0] == b'/' {
        let _ = copy_bytes(out, &cmd[..c_len(cmd)]);
    } else if contains_byte(cmd, b'/') {
        if !resolve_path(cwd, cmd, out) {
            return false;
        }
    } else {
        let dirs: [&[u8]; 3] = [b"/shell/apps/", b"/shell/apps/uwm/", b"/shell/apps/inputm/"];
        let mut fallback = [0u8; PATH_MAX];
        let mut i = 0usize;
        while i < dirs.len() {
            let mut candidate = [0u8; PATH_MAX];
            if !append_bytes(&mut candidate, dirs[i]) || !append_cstr(&mut candidate, cmd) {
                return false;
            }
            if !has_suffix(&candidate, b".elf") && !append_bytes(&mut candidate, b".elf") {
                return false;
            }
            if i == 0 {
                let _ = copy_bytes(&mut fallback, &candidate[..c_len(&candidate)]);
            }
            if syscall(SYSCALL_FS_STAT_TYPE, candidate.as_ptr() as U64, 0, 0) == FS_TYPE_FILE {
                let _ = copy_bytes(out, &candidate[..c_len(&candidate)]);
                return true;
            }
            i += 1;
        }
        let _ = copy_bytes(out, &fallback[..c_len(&fallback)]);
        return true;
    }
    if !has_suffix(out, b".elf") {
        append_bytes(out, b".elf")
    } else {
        true
    }
}

