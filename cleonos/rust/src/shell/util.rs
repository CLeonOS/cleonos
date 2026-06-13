use super::*;

pub(crate) fn c_len(buf: &[u8]) -> usize {
    let mut i = 0usize;
    while i < buf.len() && buf[i] != 0 {
        i += 1;
    }
    i
}

pub(crate) fn clear_buf(buf: &mut [u8]) {
    for byte in buf.iter_mut() {
        *byte = 0;
    }
}

pub(crate) fn copy_bytes(dst: &mut [u8], src: &[u8]) -> bool {
    if dst.is_empty() {
        return false;
    }
    let mut i = 0usize;
    while i + 1 < dst.len() && i < src.len() && src[i] != 0 {
        dst[i] = src[i];
        i += 1;
    }
    dst[i] = 0;
    i == src.len() || i < src.len() && src[i] == 0
}

pub(crate) fn copy_from_ptr(dst: &mut [u8], ptr: *const u8) -> bool {
    clear_buf(dst);
    if ptr.is_null() || dst.is_empty() {
        return false;
    }
    let mut i = 0usize;
    unsafe {
        while i + 1 < dst.len() {
            let ch = *ptr.add(i);
            if ch == 0 {
                break;
            }
            dst[i] = ch;
            i += 1;
        }
    }
    dst[i] = 0;
    true
}

pub(crate) fn append_bytes(dst: &mut [u8], src: &[u8]) -> bool {
    let mut pos = c_len(dst);
    let mut i = 0usize;
    while i < src.len() && src[i] != 0 {
        if pos + 1 >= dst.len() {
            return false;
        }
        dst[pos] = src[i];
        pos += 1;
        i += 1;
    }
    dst[pos] = 0;
    true
}

pub(crate) fn append_cstr(dst: &mut [u8], src: &[u8]) -> bool {
    append_bytes(dst, &src[..c_len(src)])
}

pub(crate) fn eq_cstr(left: &[u8], right: &[u8]) -> bool {
    let llen = c_len(left);
    let rlen = c_len(right);
    llen == rlen && left[..llen] == right[..rlen]
}

pub(crate) fn has_suffix(text: &[u8], suffix: &[u8]) -> bool {
    let len = c_len(text);
    let slen = suffix.len();
    len >= slen && text[len - slen..len] == *suffix
}

pub(crate) fn contains_byte(text: &[u8], needle: u8) -> bool {
    let mut i = 0usize;
    while i < text.len() && text[i] != 0 {
        if text[i] == needle {
            return true;
        }
        i += 1;
    }
    false
}

pub(crate) fn is_space(ch: u8) -> bool {
    matches!(ch, b' ' | b'\t' | b'\r' | b'\n' | 0x0b | 0x0c)
}

pub(crate) fn is_printable(ch: u8) -> bool {
    (ch >= 0x20 && ch < 0x7f) || ch >= 0x80
}

pub(crate) fn trim_ascii_in_place(buf: &mut [u8]) {
    let len = c_len(buf);
    let mut start = 0usize;
    let mut end = len;
    while start < end && is_space(buf[start]) {
        start += 1;
    }
    while end > start && is_space(buf[end - 1]) {
        end -= 1;
    }
    let mut i = 0usize;
    while start + i < end {
        buf[i] = buf[start + i];
        i += 1;
    }
    if i < buf.len() {
        buf[i] = 0;
    }
}

pub(crate) fn split_line(line: &[u8], cmd: &mut [u8], arg: &mut [u8]) {
    clear_buf(cmd);
    clear_buf(arg);
    let len = c_len(line);
    let mut start = 0usize;
    while start < len && is_space(line[start]) {
        start += 1;
    }
    let mut mid = start;
    while mid < len && !is_space(line[mid]) {
        mid += 1;
    }
    let _ = copy_bytes(cmd, &line[start..mid]);
    while mid < len && is_space(line[mid]) {
        mid += 1;
    }
    let _ = copy_bytes(arg, &line[mid..len]);
}

pub(crate) fn split_first_and_rest(arg: &[u8], first: &mut [u8], rest: &mut [u8]) -> bool {
    clear_buf(first);
    clear_buf(rest);
    let len = c_len(arg);
    let mut i = 0usize;
    while i < len && is_space(arg[i]) {
        i += 1;
    }
    if i >= len {
        return false;
    }
    let start = i;
    while i < len && !is_space(arg[i]) {
        i += 1;
    }
    let _ = copy_bytes(first, &arg[start..i]);
    while i < len && is_space(arg[i]) {
        i += 1;
    }
    let _ = copy_bytes(rest, &arg[i..len]);
    true
}

pub(crate) fn parse_u64_dec(text: &[u8], out: &mut U64) -> bool {
    let len = c_len(text);
    if len == 0 {
        return false;
    }
    let mut value = 0u64;
    let mut i = 0usize;
    while i < len {
        if text[i] < b'0' || text[i] > b'9' {
            return false;
        }
        let digit = (text[i] - b'0') as U64;
        if value > ((!0u64 - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        i += 1;
    }
    *out = value;
    true
}

pub(crate) fn locale_is_zh() -> bool {
    let mut locale = [0u8; 16];
    syscall(SYSCALL_LOCALE_GET, locale.as_mut_ptr() as U64, locale.len() as U64, 0) != 0
        && locale[0] == b'z'
        && locale[1] == b'h'
}

pub(crate) fn writeln_i18n(en: &[u8], zh: &[u8]) {
    if locale_is_zh() {
        writeln(zh);
    } else {
        writeln(en);
    }
}
