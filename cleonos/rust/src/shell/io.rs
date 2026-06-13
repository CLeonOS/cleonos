use super::*;

pub(crate) fn fd_write_all(fd: U64, bytes: &[u8]) -> bool {
    let mut offset = 0usize;
    while offset < bytes.len() {
        let mut chunk = bytes.len() - offset;
        if chunk > 2000 {
            chunk = 2000;
        }
        let wrote = syscall(SYSCALL_FD_WRITE, fd, bytes[offset..].as_ptr() as U64, chunk as U64);
        if wrote == 0 || wrote == !0 {
            return false;
        }
        offset += wrote as usize;
    }
    true
}

pub(crate) fn write_bytes(text: &[u8]) {
    if text.is_empty() {
        return;
    }
    if !fd_write_all(FD_STDOUT, text) {
        let _ = syscall(SYSCALL_TTY_WRITE, text.as_ptr() as U64, text.len() as U64, 0);
    }
}

pub(crate) fn write_cstr(text: &[u8]) {
    let len = c_len(text);
    write_bytes(&text[..len]);
}

pub(crate) fn writeln(text: &[u8]) {
    write_cstr(text);
    write_bytes(b"\n");
}

pub(crate) fn write_char(ch: u8) {
    let byte = [ch];
    if !fd_write_all(FD_STDOUT, &byte) {
        let _ = syscall(SYSCALL_TTY_WRITE_CHAR, ch as U64, 0, 0);
    }
}

pub(crate) fn write_u64_dec(mut value: U64) {
    let mut rev = [0u8; 24];
    let mut len = 0usize;
    if value == 0 {
        write_char(b'0');
        return;
    }
    while value > 0 && len < rev.len() {
        rev[len] = b'0' + (value % 10) as u8;
        value /= 10;
        len += 1;
    }
    while len > 0 {
        len -= 1;
        write_char(rev[len]);
    }
}

pub(crate) fn write_u64_hex(value: U64) {
    const HEX: &[u8; 16] = b"0123456789ABCDEF";
    write_bytes(b"0x");
    let mut started = false;
    let mut shift = 60i32;
    while shift >= 0 {
        let nibble = ((value >> shift) & 0xF) as usize;
        if nibble != 0 || started || shift == 0 {
            started = true;
            write_char(HEX[nibble]);
        }
        shift -= 4;
    }
}

pub(crate) fn print_kv_dec(label: &[u8], value: U64) {
    write_cstr(label);
    write_bytes(b": ");
    write_u64_dec(value);
    write_bytes(b"\n");
}

pub(crate) fn print_kv_hex(label: &[u8], value: U64) {
    write_cstr(label);
    write_bytes(b": ");
    write_u64_hex(value);
    write_bytes(b"\n");
}

pub(crate) fn read_char_blocking() -> u8 {
    let mut ch = [0u8; 1];
    loop {
        if syscall(SYSCALL_FD_READ, FD_STDIN, ch.as_mut_ptr() as U64, 1) == 1 {
            return ch[0];
        }
        let _ = syscall(SYSCALL_YIELD, 0, 0, 0);
        let _ = syscall(SYSCALL_SLEEP_MS, 1, 0, 0);
    }
}

pub(crate) fn read_plain_line(prompt: &[u8], out: &mut [u8], secret: bool) {
    clear_buf(out);
    write_cstr(prompt);
    let mut len = 0usize;
    loop {
        let byte = read_char_blocking();
        if byte == b'\r' {
            continue;
        }
        if byte == b'\r' || byte == b'\n' {
            write_bytes(b"\n");
            break;
        }
        if byte == 8 || byte == 127 {
            if len > 0 {
                len -= 1;
                out[len] = 0;
                if !secret {
                    write_bytes(b"\x08 \x08");
                }
            }
            continue;
        }
        if byte == 0 || byte == b'\t' {
            continue;
        }
        if len + 1 >= out.len() {
            continue;
        }
        out[len] = byte;
        len += 1;
        out[len] = 0;
        if !secret {
            write_char(byte);
        }
    }
}
