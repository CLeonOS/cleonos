use core::ptr;

type U64 = u64;

const SYSCALL_FS_READ: U64 = 12;
const SYSCALL_FS_CHILD_COUNT: U64 = 10;
const SYSCALL_FS_GET_CHILD_NAME: U64 = 11;
const SYSCALL_EXEC_PATHV_IO: U64 = 80;
const SYSCALL_TTY_WRITE: U64 = 24;
const SYSCALL_TTY_WRITE_CHAR: U64 = 25;
const SYSCALL_FS_STAT_TYPE: U64 = 27;
const SYSCALL_FS_WRITE: U64 = 30;
const SYSCALL_FS_REMOVE: U64 = 32;
const SYSCALL_YIELD: U64 = 45;
const SYSCALL_FD_OPEN: U64 = 72;
const SYSCALL_FD_READ: U64 = 73;
const SYSCALL_FD_WRITE: U64 = 74;
const SYSCALL_FD_CLOSE: U64 = 75;
const SYSCALL_SLEEP_MS: U64 = 128;
const SYSCALL_PROC_ENVC: U64 = 55;
const SYSCALL_PROC_ENV: U64 = 56;
const SYSCALL_USER_SHELL_READY: U64 = 16;
const SYSCALL_USER_CURRENT: U64 = 132;
const SYSCALL_USER_LOGIN: U64 = 133;
const SYSCALL_USER_LOGOUT: U64 = 134;
const SYSCALL_LOCALE_GET: U64 = 144;

const PATH_MAX: usize = 192;
const LINE_MAX: usize = 192;
const ARG_MAX: usize = 160;
const ENV_MAX: usize = 512;
const NAME_MAX: usize = 32;
const HOME_MAX: usize = 96;
const FS_NAME_MAX: usize = 96;
const HISTORY_MAX: usize = 16;
const HISTORY_DATA_MAX: usize = 4096;
const SCRIPT_MAX: usize = 1024;
const MATCH_MAX: usize = 48;
const MATCH_DISPLAY_MAX: usize = 8;
const PIPELINE_MAX_STAGES: usize = 8;
const CMD_CTX_PATH: &[u8] = b"/temp/.ush_cmd_ctx.bin\0";
const CMD_RET_PATH: &[u8] = b"/temp/.ush_cmd_ret.bin\0";
const HISTORY_DIR: &[u8] = b"/shell/data\0";
const HISTORY_PATH: &[u8] = b"/shell/data/history.txt\0";
const HISTORY_FALLBACK_PATH: &[u8] = b"/temp/shell_history.txt\0";
const PIPE_TMP_A: &[u8] = b"/temp/.ush_pipe_a.bin\0";
const PIPE_TMP_B: &[u8] = b"/temp/.ush_pipe_b.bin\0";
const FD_INHERIT: U64 = !0;
const FD_STDIN: U64 = 0;
const FD_STDOUT: U64 = 1;
const O_RDONLY: U64 = 0x0000;
const O_WRONLY: U64 = 0x0001;
const O_CREAT: U64 = 0x0040;
const O_TRUNC: U64 = 0x0200;
const O_APPEND: U64 = 0x0400;
const USER_ROLE_ADMIN: U64 = 1;
const CMD_RET_FLAG_CWD: U64 = 0x1;
const CMD_RET_FLAG_EXIT: U64 = 0x2;
const FS_TYPE_FILE: U64 = 1;
const FS_TYPE_DIR: U64 = 2;

const KEY_LEFT: u8 = 0x01;
const KEY_RIGHT: u8 = 0x02;
const KEY_UP: u8 = 0x03;
const KEY_DOWN: u8 = 0x04;
const KEY_HOME: u8 = 0x05;
const KEY_END: u8 = 0x06;
const KEY_DELETE: u8 = 0x07;
const KEY_SELECT_ALL: u8 = 0x10;
const KEY_COPY: u8 = 0x11;
const KEY_PASTE: u8 = 0x12;
const KEY_SHIFT_LEFT: u8 = 0x13;
const KEY_SHIFT_RIGHT: u8 = 0x14;
const KEY_SHIFT_HOME: u8 = 0x15;
const KEY_SHIFT_END: u8 = 0x16;
const KEY_REVERSE_SEARCH: u8 = 0x17;
const KEY_LINE_START: u8 = 0x18;
const KEY_LINE_END: u8 = 0x19;
const KEY_KILL_BEFORE: u8 = 0x1A;
const KEY_KILL_AFTER: u8 = 0x1C;
const KEY_KILL_WORD_BEFORE: u8 = 0x1D;
const KEY_CLEAR_SCREEN: u8 = 0x1E;
const KEY_EOF_OR_DELETE: u8 = 0x1F;
const COMPLETE_NONE: U64 = 0;
const COMPLETE_EDITED: U64 = 1;
const COMPLETE_LISTED: U64 = 2;

const COMMANDS: &[&[u8]] = &[
    b".", b"ansi", b"ansitest", b"append", b"args", b"bdt", b"benchmark", b"bg", b"bmpview", b"bootargs",
    b"browser", b"calc", b"calendar", b"cat", b"cd", b"chinese", b"clear", b"clksd", b"cls", b"color",
    b"contacts", b"control", b"cp", b"cut", b"devtest", b"dir", b"diskinfo", b"dltest", b"dmesg", b"doom",
    b"drvctl", b"emoji", b"exec", b"exit", b"fastfetch", b"fdtest", b"fg", b"file_explorer", b"fsckfat32",
    b"fsstat", b"grep", b"hbos", b"head", b"hello", b"help", b"httpd", b"httpget", b"ifconfig", b"imgview",
    b"install2disk", b"jobs", b"kbdstat", b"kdbg", b"kill", b"leonfetch", b"libctest", b"libdemo", b"locale",
    b"logout", b"ls", b"lua", b"memstat", b"mkdir", b"mkfsfat32", b"mount", b"mv", b"note", b"nslookup",
    b"partctl", b"passwd", b"pid", b"ping", b"pinyin", b"pkg", b"pkg_gui", b"pngtest", b"poweroff",
    b"procstat", b"ps", b"pwd", b"qrcode", b"reboot", b"resolution", b"restart", b"rm", b"romaji", b"rsh",
    b"rshd", b"run", b"shell", b"shstat", b"shutdown", b"sleep", b"sort", b"source", b"spawn", b"spin",
    b"sqlitetest", b"stardust_helloworld", b"stardust_layout", b"stats", b"stbtest", b"symbols", b"sysinfo",
    b"sysstat", b"systemctl", b"tail", b"taskmgr", b"taskstat", b"tcc", b"terminal", b"termbox2",
    b"termboxdemo", b"timertest", b"todo", b"top", b"touch", b"ttftest", b"tty", b"tui", b"tuitest",
    b"uname", b"uniq", b"unzip", b"usc-agent", b"useradd", b"userdel", b"usermod", b"users", b"userstat",
    b"uwm", b"uwm_uilib", b"vim", b"vmtest", b"wait", b"wavplay", b"wc", b"webconsole", b"wget", b"whoami",
    b"write", b"yield", b"zip", b"zlibtest",
];

#[repr(C)]
struct UserInfo {
    uid: U64,
    role: U64,
    logged_in: U64,
    disk_login_required: U64,
    name: [u8; NAME_MAX],
    home: [u8; HOME_MAX],
}

#[repr(C)]
struct UserLoginReq {
    name_ptr: U64,
    password_ptr: U64,
    out_info_ptr: U64,
}

#[repr(C)]
struct CmdCtx {
    cmd: [u8; 32],
    arg: [u8; ARG_MAX],
    cwd: [u8; PATH_MAX],
}

#[repr(C)]
struct CmdRet {
    flags: U64,
    exit_code: U64,
    cwd: [u8; PATH_MAX],
}

#[repr(C)]
struct ExecPathvIoReq {
    env_line_ptr: U64,
    stdin_fd: U64,
    stdout_fd: U64,
    stderr_fd: U64,
}

struct ShellState {
    line: [u8; LINE_MAX],
    line_len: usize,
    cursor: usize,
    rendered_len: usize,

    cwd: [u8; PATH_MAX],
    username: [u8; NAME_MAX],
    home: [u8; HOME_MAX],
    role: U64,
    disk_login_required: bool,
    logged_in: bool,

    history: [[u8; LINE_MAX]; HISTORY_MAX],
    history_count: usize,
    history_nav: i64,
    nav_saved_line: [u8; LINE_MAX],
    nav_saved_len: usize,
    nav_saved_cursor: usize,

    clipboard: [u8; LINE_MAX],
    clipboard_len: usize,
    sel_start: usize,
    sel_end: usize,
    sel_active: bool,
    sel_anchor: usize,
    sel_anchor_valid: bool,

    cmd_total: U64,
    cmd_ok: U64,
    cmd_fail: U64,
    cmd_unknown: U64,
    exit_requested: bool,
    exit_code: U64,
}

#[derive(Clone, Copy)]
struct ExecResult {
    known: bool,
    success: bool,
}

#[derive(Clone, Copy)]
struct PipelineStage {
    text: [u8; LINE_MAX],
    cmd: [u8; 32],
    arg: [u8; ARG_MAX],
    redirect_path: [u8; PATH_MAX],
    redirect_mode: u8,
}

struct MatchList {
    items: [[u8; PATH_MAX]; MATCH_MAX],
    count: usize,
}

extern "C" {
    fn cleonos_syscall(id: U64, arg0: U64, arg1: U64, arg2: U64) -> U64;
}

fn syscall(id: U64, arg0: U64, arg1: U64, arg2: U64) -> U64 {
    unsafe { cleonos_syscall(id, arg0, arg1, arg2) }
}

fn c_len(buf: &[u8]) -> usize {
    let mut i = 0usize;
    while i < buf.len() && buf[i] != 0 {
        i += 1;
    }
    i
}

fn fd_write_all(fd: U64, bytes: &[u8]) -> bool {
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

fn write_bytes(text: &[u8]) {
    if text.is_empty() {
        return;
    }
    if !fd_write_all(FD_STDOUT, text) {
        let _ = syscall(SYSCALL_TTY_WRITE, text.as_ptr() as U64, text.len() as U64, 0);
    }
}

fn write_cstr(text: &[u8]) {
    let len = c_len(text);
    write_bytes(&text[..len]);
}

fn writeln(text: &[u8]) {
    write_cstr(text);
    write_bytes(b"\n");
}

fn write_char(ch: u8) {
    let byte = [ch];
    if !fd_write_all(FD_STDOUT, &byte) {
        let _ = syscall(SYSCALL_TTY_WRITE_CHAR, ch as U64, 0, 0);
    }
}

fn write_u64_dec(mut value: U64) {
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

fn write_u64_hex(value: U64) {
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

fn print_kv_dec(label: &[u8], value: U64) {
    write_cstr(label);
    write_bytes(b": ");
    write_u64_dec(value);
    write_bytes(b"\n");
}

fn print_kv_hex(label: &[u8], value: U64) {
    write_cstr(label);
    write_bytes(b": ");
    write_u64_hex(value);
    write_bytes(b"\n");
}

fn clear_buf(buf: &mut [u8]) {
    for byte in buf.iter_mut() {
        *byte = 0;
    }
}

fn copy_bytes(dst: &mut [u8], src: &[u8]) -> bool {
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

fn copy_from_ptr(dst: &mut [u8], ptr: *const u8) -> bool {
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

fn append_bytes(dst: &mut [u8], src: &[u8]) -> bool {
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

fn append_cstr(dst: &mut [u8], src: &[u8]) -> bool {
    append_bytes(dst, &src[..c_len(src)])
}

fn eq_cstr(left: &[u8], right: &[u8]) -> bool {
    let llen = c_len(left);
    let rlen = c_len(right);
    llen == rlen && left[..llen] == right[..rlen]
}

fn has_suffix(text: &[u8], suffix: &[u8]) -> bool {
    let len = c_len(text);
    let slen = suffix.len();
    len >= slen && text[len - slen..len] == *suffix
}

fn contains_byte(text: &[u8], needle: u8) -> bool {
    let mut i = 0usize;
    while i < text.len() && text[i] != 0 {
        if text[i] == needle {
            return true;
        }
        i += 1;
    }
    false
}

fn is_space(ch: u8) -> bool {
    matches!(ch, b' ' | b'\t' | b'\r' | b'\n' | 0x0b | 0x0c)
}

fn is_printable(ch: u8) -> bool {
    (ch >= 0x20 && ch < 0x7f) || ch >= 0x80
}

fn trim_ascii_in_place(buf: &mut [u8]) {
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

fn split_line(line: &[u8], cmd: &mut [u8], arg: &mut [u8]) {
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

fn split_first_and_rest(arg: &[u8], first: &mut [u8], rest: &mut [u8]) -> bool {
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

fn parse_u64_dec(text: &[u8], out: &mut U64) -> bool {
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

fn locale_is_zh() -> bool {
    let mut locale = [0u8; 16];
    syscall(SYSCALL_LOCALE_GET, locale.as_mut_ptr() as U64, locale.len() as U64, 0) != 0
        && locale[0] == b'z'
        && locale[1] == b'h'
}

fn writeln_i18n(en: &[u8], zh: &[u8]) {
    if locale_is_zh() {
        writeln(zh);
    } else {
        writeln(en);
    }
}

fn read_char_blocking() -> u8 {
    let mut ch = [0u8; 1];
    loop {
        if syscall(SYSCALL_FD_READ, FD_STDIN, ch.as_mut_ptr() as U64, 1) == 1 {
            return ch[0];
        }
        let _ = syscall(SYSCALL_YIELD, 0, 0, 0);
        let _ = syscall(SYSCALL_SLEEP_MS, 1, 0, 0);
    }
}

fn read_plain_line(prompt: &[u8], out: &mut [u8], secret: bool) {
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

fn utf8_is_cont(ch: u8) -> bool {
    ch >= 0x80 && ch <= 0xBF
}

fn utf8_len_from_lead(lead: u8) -> usize {
    if lead < 0x80 {
        1
    } else if lead >= 0xC2 && lead <= 0xDF {
        2
    } else if lead >= 0xE0 && lead <= 0xEF {
        3
    } else if lead >= 0xF0 && lead <= 0xF4 {
        4
    } else {
        1
    }
}

fn utf8_char_len_at(text: &[u8], len: usize, pos: usize) -> usize {
    if pos >= len {
        return 0;
    }
    let need = utf8_len_from_lead(text[pos]);
    if need == 1 || pos + need > len {
        return 1;
    }
    let mut i = 1usize;
    while i < need {
        if !utf8_is_cont(text[pos + i]) {
            return 1;
        }
        i += 1;
    }
    need
}

fn utf8_prev_boundary(text: &[u8], len: usize, mut pos: usize) -> usize {
    if pos == 0 {
        return 0;
    }
    if pos > len {
        pos = len;
    }
    pos -= 1;
    while pos > 0 && utf8_is_cont(text[pos]) {
        pos -= 1;
    }
    pos
}

fn utf8_next_boundary(text: &[u8], len: usize, pos: usize) -> usize {
    if pos >= len {
        return len;
    }
    let adv = utf8_char_len_at(text, len, pos);
    if adv == 0 || pos + adv > len {
        len
    } else {
        pos + adv
    }
}

fn utf8_decode_at(text: &[u8], len: usize, pos: usize, out_adv: &mut usize) -> u32 {
    *out_adv = 0;
    if pos >= len {
        return 0;
    }
    let b0 = text[pos];
    let adv = utf8_char_len_at(text, len, pos);
    *out_adv = adv;
    if adv == 1 {
        return b0 as u32;
    }
    if adv == 2 {
        return (((b0 & 0x1F) as u32) << 6) | ((text[pos + 1] & 0x3F) as u32);
    }
    if adv == 3 {
        return (((b0 & 0x0F) as u32) << 12)
            | (((text[pos + 1] & 0x3F) as u32) << 6)
            | ((text[pos + 2] & 0x3F) as u32);
    }
    (((b0 & 0x07) as u32) << 18)
        | (((text[pos + 1] & 0x3F) as u32) << 12)
        | (((text[pos + 2] & 0x3F) as u32) << 6)
        | ((text[pos + 3] & 0x3F) as u32)
}

fn codepoint_width(cp: u32) -> usize {
    if cp == 0 {
        0
    } else if (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0xFE00 && cp <= 0xFE0F) {
        0
    } else if (cp >= 0x1100 && cp <= 0x115F)
        || (cp >= 0x2E80 && cp <= 0xA4CF)
        || (cp >= 0xAC00 && cp <= 0xD7A3)
        || (cp >= 0xF900 && cp <= 0xFAFF)
        || (cp >= 0xFE10 && cp <= 0xFE19)
        || (cp >= 0xFE30 && cp <= 0xFE6F)
        || (cp >= 0xFF00 && cp <= 0xFF60)
        || (cp >= 0xFFE0 && cp <= 0xFFE6)
        || (cp >= 0x20000 && cp <= 0x3FFFD)
    {
        2
    } else {
        1
    }
}

fn utf8_visual_width(text: &[u8], len: usize) -> usize {
    let mut pos = 0usize;
    let mut cols = 0usize;
    while pos < len {
        let mut adv = 1usize;
        let cp = utf8_decode_at(text, len, pos, &mut adv);
        if adv == 0 {
            break;
        }
        cols += codepoint_width(cp);
        pos += adv;
    }
    cols
}

fn collect_utf8(first: u8, out: &mut [u8; 4]) -> usize {
    out[0] = first;
    let need = utf8_len_from_lead(first);
    let mut len = 1usize;
    while len < need && len < out.len() {
        let next = read_char_blocking();
        out[len] = next;
        len += 1;
        if !utf8_is_cont(next) {
            break;
        }
    }
    len
}

fn reset_line(sh: &mut ShellState) {
    sh.line_len = 0;
    sh.cursor = 0;
    sh.rendered_len = 0;
    clear_buf(&mut sh.line);
    selection_clear(sh);
}

fn insert_text(sh: &mut ShellState, text: &[u8]) {
    if text.is_empty() {
        return;
    }
    if sh.cursor > sh.line_len {
        sh.cursor = sh.line_len;
    }
    let mut text_len = text.len();
    let available = (LINE_MAX - 1) - sh.line_len;
    if text_len > available {
        text_len = available;
    }
    if text_len == 0 {
        return;
    }
    let mut i = sh.line_len + 1;
    while i > sh.cursor {
        sh.line[i + text_len - 1] = sh.line[i - 1];
        i -= 1;
    }
    let mut j = 0usize;
    while j < text_len {
        sh.line[sh.cursor + j] = text[j];
        j += 1;
    }
    sh.line_len += text_len;
    sh.cursor += text_len;
    sh.line[sh.line_len] = 0;
}

fn delete_range(sh: &mut ShellState, start: usize, mut end: usize) {
    if start >= end || start >= sh.line_len {
        selection_clear(sh);
        return;
    }
    if end > sh.line_len {
        end = sh.line_len;
    }
    let delta = end - start;
    let mut i = start;
    while i + delta <= sh.line_len {
        sh.line[i] = sh.line[i + delta];
        i += 1;
    }
    sh.line_len -= delta;
    if sh.cursor > end {
        sh.cursor -= delta;
    } else if sh.cursor > start {
        sh.cursor = start;
    }
    if sh.cursor > sh.line_len {
        sh.cursor = sh.line_len;
    }
    selection_clear(sh);
}

fn selection_clear(sh: &mut ShellState) {
    sh.sel_active = false;
    sh.sel_start = 0;
    sh.sel_end = 0;
    sh.sel_anchor = 0;
    sh.sel_anchor_valid = false;
}

fn selection_select_all(sh: &mut ShellState) {
    if sh.line_len == 0 {
        selection_clear(sh);
        return;
    }
    sh.sel_active = true;
    sh.sel_start = 0;
    sh.sel_end = sh.line_len;
    sh.sel_anchor = 0;
    sh.sel_anchor_valid = true;
}

fn selection_update_from_anchor(sh: &mut ShellState) {
    if !sh.sel_anchor_valid {
        sh.sel_active = false;
        return;
    }
    let mut anchor = sh.sel_anchor;
    if anchor > sh.line_len {
        anchor = sh.line_len;
    }
    if sh.cursor == anchor {
        sh.sel_active = false;
        sh.sel_start = anchor;
        sh.sel_end = anchor;
        return;
    }
    sh.sel_active = true;
    if sh.cursor < anchor {
        sh.sel_start = sh.cursor;
        sh.sel_end = anchor;
    } else {
        sh.sel_start = anchor;
        sh.sel_end = sh.cursor;
    }
}

fn selection_range(sh: &ShellState, out_start: &mut usize, out_end: &mut usize) -> bool {
    if !sh.sel_active {
        return false;
    }
    let mut start = sh.sel_start;
    let mut end = sh.sel_end;
    if start > sh.line_len {
        start = sh.line_len;
    }
    if end > sh.line_len {
        end = sh.line_len;
    }
    if start > end {
        let tmp = start;
        start = end;
        end = tmp;
    }
    if start == end {
        return false;
    }
    *out_start = start;
    *out_end = end;
    true
}

fn copy_selection(sh: &mut ShellState) {
    let mut start = 0usize;
    let mut end = 0usize;
    if !selection_range(sh, &mut start, &mut end) {
        return;
    }
    let mut len = end - start;
    if len > LINE_MAX - 1 {
        len = LINE_MAX - 1;
    }
    let mut i = 0usize;
    while i < len {
        sh.clipboard[i] = sh.line[start + i];
        i += 1;
    }
    sh.clipboard[len] = 0;
    sh.clipboard_len = len;
}

fn delete_selection(sh: &mut ShellState) -> bool {
    let mut start = 0usize;
    let mut end = 0usize;
    if !selection_range(sh, &mut start, &mut end) {
        return false;
    }
    delete_range(sh, start, end);
    true
}

fn cut_range_to_clipboard(sh: &mut ShellState, start: usize, mut end: usize) {
    if start >= end || start >= sh.line_len {
        return;
    }
    if end > sh.line_len {
        end = sh.line_len;
    }
    let mut len = end - start;
    if len > LINE_MAX - 1 {
        len = LINE_MAX - 1;
    }
    let mut i = 0usize;
    while i < len {
        sh.clipboard[i] = sh.line[start + i];
        i += 1;
    }
    sh.clipboard[len] = 0;
    sh.clipboard_len = len;
    delete_range(sh, start, end);
}

fn prev_word_boundary(text: &[u8], len: usize, mut pos: usize) -> usize {
    if pos > len {
        pos = len;
    }
    while pos > 0 {
        let prev = utf8_prev_boundary(text, len, pos);
        if prev == pos || !is_space(text[prev]) {
            break;
        }
        pos = prev;
    }
    while pos > 0 {
        let prev = utf8_prev_boundary(text, len, pos);
        if prev == pos || is_space(text[prev]) {
            break;
        }
        pos = prev;
    }
    pos
}

fn line_needs_continuation(line: &[u8], len: usize) -> bool {
    if len == 0 {
        return false;
    }
    let mut pos = len;
    while pos > 0 && is_space(line[pos - 1]) {
        pos -= 1;
    }
    pos > 0 && line[pos - 1] == b'\\'
}

fn remove_continuation_marker(sh: &mut ShellState) {
    let mut pos = sh.line_len;
    while pos > 0 && is_space(sh.line[pos - 1]) {
        pos -= 1;
    }
    if pos > 0 && sh.line[pos - 1] == b'\\' {
        delete_range(sh, pos - 1, pos);
        sh.cursor = sh.line_len;
    }
}

fn path_push(path: &mut [u8], io_len: &mut usize, component: &[u8]) -> bool {
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

fn path_pop(path: &mut [u8], io_len: &mut usize) {
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

fn path_parse_into(src: &[u8], out: &mut [u8], io_len: &mut usize) -> bool {
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

fn resolve_path(cwd: &[u8], arg: &[u8], out: &mut [u8]) -> bool {
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

fn resolve_exec_path(cwd: &[u8], cmd: &[u8], out: &mut [u8]) -> bool {
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

fn env_value(key: &[u8], out: &mut [u8]) -> bool {
    clear_buf(out);
    let envc = syscall(SYSCALL_PROC_ENVC, 0, 0, 0);
    let key_len = key.len();
    let mut i = 0u64;
    let mut item = [0u8; 128];
    while i < envc {
        clear_buf(&mut item);
        if syscall(SYSCALL_PROC_ENV, i, item.as_mut_ptr() as U64, item.len() as U64) != 0 {
            let item_len = c_len(&item);
            if item_len > key_len && item[..key_len] == *key && item[key_len] == b'=' {
                let _ = copy_bytes(out, &item[key_len + 1..item_len]);
                return true;
            }
        }
        i += 1;
    }
    false
}

fn apply_user_info(sh: &mut ShellState, info: &UserInfo) {
    let _ = copy_bytes(&mut sh.username, &info.name[..c_len(&info.name)]);
    let _ = copy_bytes(&mut sh.home, &info.home[..c_len(&info.home)]);
    sh.role = info.role;
    sh.disk_login_required = info.disk_login_required != 0;
    sh.logged_in = info.logged_in != 0;
    if info.home[0] == b'/' && syscall(SYSCALL_FS_STAT_TYPE, info.home.as_ptr() as U64, 0, 0) == 2 {
        let _ = copy_bytes(&mut sh.cwd, &info.home[..c_len(&info.home)]);
    } else {
        let _ = copy_bytes(&mut sh.cwd, b"/");
    }
}

fn login_if_needed(sh: &mut ShellState) -> bool {
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
            apply_user_info(sh, &info);
            return true;
        }
    }
    if !sh.disk_login_required {
        let name = b"root\0";
        let password = b"\0";
        let mut login_info = UserInfo {
            uid: 0,
            role: 0,
            logged_in: 0,
            disk_login_required: 0,
            name: [0; NAME_MAX],
            home: [0; HOME_MAX],
        };
        let req = UserLoginReq {
            name_ptr: name.as_ptr() as U64,
            password_ptr: password.as_ptr() as U64,
            out_info_ptr: &mut login_info as *mut _ as U64,
        };
        let _ = syscall(SYSCALL_USER_LOGIN, &req as *const _ as U64, 0, 0);
        let _ = copy_bytes(&mut sh.username, b"root");
        let _ = copy_bytes(&mut sh.home, b"/");
        let _ = copy_bytes(&mut sh.cwd, b"/");
        sh.role = USER_ROLE_ADMIN;
        sh.logged_in = true;
        return true;
    }

    writeln(b"CLeonOS disk login\0");
    loop {
        let mut name = [0u8; NAME_MAX];
        let mut password = [0u8; 96];
        read_plain_line(b"login: ", &mut name, false);
        trim_ascii_in_place(&mut name);
        if c_len(&name) == 0 {
            continue;
        }
        read_plain_line(b"password: ", &mut password, true);
        let mut login_info = UserInfo {
            uid: 0,
            role: 0,
            logged_in: 0,
            disk_login_required: 0,
            name: [0; NAME_MAX],
            home: [0; HOME_MAX],
        };
        let req = UserLoginReq {
            name_ptr: name.as_ptr() as U64,
            password_ptr: password.as_ptr() as U64,
            out_info_ptr: &mut login_info as *mut _ as U64,
        };
        if syscall(SYSCALL_USER_LOGIN, &req as *const _ as U64, 0, 0) != 0 {
            apply_user_info(sh, &login_info);
            write_bytes(b"login: welcome ");
            write_cstr(&login_info.name);
            write_bytes(b"\n");
            return true;
        }
        writeln(b"login: invalid username or password\0");
    }
}

fn write_prompt(sh: &ShellState) {
    let mut render = [0u8; 320];
    let mut len = 0usize;
    render_append_prompt(sh, &mut render, &mut len);
    write_bytes(&render[..len]);
}

fn render_append(out: &mut [u8], len: &mut usize, text: &[u8]) {
    let mut i = 0usize;
    while i < text.len() && text[i] != 0 {
        if *len + 1 >= out.len() {
            return;
        }
        out[*len] = text[i];
        *len += 1;
        i += 1;
    }
}

fn render_append_prompt(sh: &ShellState, out: &mut [u8], len: &mut usize) {
    render_append(out, len, b"\x1B[96mcleonos\x1B[0m(\x1B[92m");
    if c_len(&sh.username) == 0 {
        render_append(out, len, b"user");
    } else {
        render_append(out, len, &sh.username[..c_len(&sh.username)]);
    }
    render_append(out, len, b"\x1B[0m:\x1B[93m");
    render_append(out, len, &sh.cwd[..c_len(&sh.cwd)]);
    render_append(out, len, b"\x1B[0m)> ");
}

fn render_line_segment(sh: &ShellState, limit: usize, out: &mut [u8], out_len: &mut usize) {
    let mut i = 0usize;
    let mut inverse = false;
    let mut sel_start = 0usize;
    let mut sel_end = 0usize;
    let has_sel = selection_range(sh, &mut sel_start, &mut sel_end);
    let real_limit = if limit > sh.line_len { sh.line_len } else { limit };
    while i < real_limit {
        if has_sel && !inverse && i == sel_start {
            render_append(out, out_len, b"\x1B[7m");
            inverse = true;
        }
        if has_sel && inverse && i == sel_end {
            render_append(out, out_len, b"\x1B[27m");
            inverse = false;
        }
        if *out_len + 1 >= out.len() {
            break;
        }
        out[*out_len] = sh.line[i];
        *out_len += 1;
        i += 1;
    }
    if inverse {
        render_append(out, out_len, b"\x1B[27m");
    }
}

fn linenoise_hint(sh: &ShellState, hint: &mut [u8]) {
    clear_buf(hint);
    if sh.line_len == 0 || sh.cursor != sh.line_len {
        return;
    }
    let mut cmd = [0u8; 32];
    let mut arg = [0u8; ARG_MAX];
    split_line(&sh.line, &mut cmd, &mut arg);
    if c_len(&cmd) == 0 {
        let _ = copy_bytes(hint, b"help");
    } else if eq_cstr(&cmd, b"pkg\0") && c_len(&arg) == 0 {
        let _ = copy_bytes(hint, b" install | list | search | update | upgrade");
    } else if eq_cstr(&cmd, b"cd\0") && c_len(&arg) == 0 {
        let _ = copy_bytes(hint, b" /system /shell /home /temp");
    } else if (eq_cstr(&cmd, b"exec\0") || eq_cstr(&cmd, b"run\0")) && c_len(&arg) == 0 {
        let _ = copy_bytes(hint, b" /shell/apps/<app>.elf");
    } else if eq_cstr(&cmd, b"browser\0") && c_len(&arg) == 0 {
        let _ = copy_bytes(hint, b" http://example.com");
    } else if eq_cstr(&cmd, b"wget\0") && c_len(&arg) == 0 {
        let _ = copy_bytes(hint, b" <url> [-o file]");
    } else if eq_cstr(&cmd, b"lua\0") && c_len(&arg) == 0 {
        let _ = copy_bytes(hint, b" [script.lua]");
    } else {
        for command in COMMANDS.iter() {
            let cmd_len = c_len(&cmd);
            if command.len() > cmd_len && command[..cmd_len] == cmd[..cmd_len] {
                let _ = copy_bytes(hint, &command[cmd_len..]);
                break;
            }
        }
    }
}

fn render_line(sh: &mut ShellState) {
    let mut render = [0u8; 4096];
    let mut out_len = 0usize;
    let mut hint = [0u8; 96];
    linenoise_hint(sh, &mut hint);
    let line_cols = utf8_visual_width(&sh.line, sh.line_len);
    let total_cols = line_cols + c_len(&hint);

    render_append(&mut render, &mut out_len, b"\r");
    render_append_prompt(sh, &mut render, &mut out_len);
    render_line_segment(sh, sh.line_len, &mut render, &mut out_len);
    if c_len(&hint) > 0 {
        render_append(&mut render, &mut out_len, b"\x1B[90m");
        render_append(&mut render, &mut out_len, &hint[..c_len(&hint)]);
        render_append(&mut render, &mut out_len, b"\x1B[0m");
    }
    let mut i = total_cols;
    while i < sh.rendered_len {
        render_append(&mut render, &mut out_len, b" ");
        i += 1;
    }
    render_append(&mut render, &mut out_len, b"\r");
    render_append_prompt(sh, &mut render, &mut out_len);
    render_line_segment(sh, sh.cursor, &mut render, &mut out_len);
    write_bytes(&render[..out_len]);
    sh.rendered_len = total_cols;
}

fn history_cancel_nav(sh: &mut ShellState) {
    sh.history_nav = -1;
    sh.nav_saved_len = 0;
    sh.nav_saved_cursor = 0;
    clear_buf(&mut sh.nav_saved_line);
}

fn line_has_non_space(line: &[u8]) -> bool {
    let mut i = 0usize;
    while i < line.len() && line[i] != 0 {
        if !is_space(line[i]) {
            return true;
        }
        i += 1;
    }
    false
}

fn history_push_memory(sh: &mut ShellState, line: &[u8]) {
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

fn history_storage_path() -> &'static [u8] {
    if syscall(SYSCALL_FS_STAT_TYPE, HISTORY_DIR.as_ptr() as U64, 0, 0) == FS_TYPE_DIR {
        HISTORY_PATH
    } else {
        HISTORY_FALLBACK_PATH
    }
}

fn history_load(sh: &mut ShellState) {
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

fn history_save(sh: &ShellState) {
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

fn history_push(sh: &mut ShellState, line: &[u8]) {
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

fn load_line(sh: &mut ShellState, line: &[u8]) {
    let _ = copy_bytes(&mut sh.line, &line[..c_len(line)]);
    sh.line_len = c_len(&sh.line);
    sh.cursor = sh.line_len;
}

fn history_apply_current(sh: &mut ShellState) {
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

fn history_up(sh: &mut ShellState) {
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

fn history_down(sh: &mut ShellState) {
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

fn join_path(dir_path: &[u8], name: &[u8], out: &mut [u8]) -> bool {
    clear_buf(out);
    if c_len(dir_path) == 0 || c_len(name) == 0 {
        return false;
    }
    let dir_len = c_len(dir_path);
    if dir_len == 1 && dir_path[0] == b'/' {
        if !append_bytes(out, b"/") {
            return false;
        }
    } else if !append_cstr(out, dir_path) || !append_bytes(out, b"/") {
        return false;
    }
    append_cstr(out, name)
}

fn has_prefix_token(text: &[u8], prefix: &[u8]) -> bool {
    let text_len = c_len(text);
    let prefix_len = c_len(prefix);
    text_len >= prefix_len && text[..prefix_len] == prefix[..prefix_len]
}

fn contains_substr(text: &[u8], needle: &[u8]) -> bool {
    let len = c_len(text);
    let nlen = c_len(needle);
    if nlen == 0 {
        return true;
    }
    if nlen > len {
        return false;
    }
    let mut i = 0usize;
    while i + nlen <= len {
        if text[i..i + nlen] == needle[..nlen] {
            return true;
        }
        i += 1;
    }
    false
}

fn match_list_clear(matches: &mut MatchList) {
    matches.count = 0;
    let mut i = 0usize;
    while i < MATCH_MAX {
        clear_buf(&mut matches.items[i]);
        i += 1;
    }
}

fn match_add(matches: &mut MatchList, text: &[u8]) {
    if c_len(text) == 0 || matches.count >= MATCH_MAX {
        return;
    }
    let mut i = 0usize;
    while i < matches.count {
        if eq_cstr(&matches.items[i], text) {
            return;
        }
        i += 1;
    }
    let idx = matches.count;
    let _ = copy_bytes(&mut matches.items[idx], &text[..c_len(text)]);
    matches.count += 1;
}

fn complete_commands(token: &[u8], matches: &mut MatchList) {
    for command in COMMANDS.iter() {
        if command.len() >= c_len(token) && command[..c_len(token)] == token[..c_len(token)] {
            match_add(matches, command);
        }
    }
}

fn complete_elf_dir(dir: &[u8], token: &[u8], matches: &mut MatchList) {
    if syscall(SYSCALL_FS_STAT_TYPE, dir.as_ptr() as U64, 0, 0) != FS_TYPE_DIR {
        return;
    }
    let count = syscall(SYSCALL_FS_CHILD_COUNT, dir.as_ptr() as U64, 0, 0);
    let mut i = 0u64;
    while i < count && matches.count < MATCH_MAX {
        let mut name = [0u8; FS_NAME_MAX];
        if syscall(SYSCALL_FS_GET_CHILD_NAME, dir.as_ptr() as U64, i, name.as_mut_ptr() as U64) != 0 {
            let len = c_len(&name);
            if len > 4 && name[len - 4..len] == *b".elf" {
                let mut command = [0u8; PATH_MAX];
                let _ = copy_bytes(&mut command, &name[..len - 4]);
                if has_prefix_token(&command, token) {
                    match_add(matches, &command);
                }
            }
        }
        i += 1;
    }
}

fn complete_external_commands(token: &[u8], matches: &mut MatchList) {
    complete_elf_dir(b"/shell/apps\0", token, matches);
    complete_elf_dir(b"/shell/apps/uwm\0", token, matches);
    complete_elf_dir(b"/shell/apps/inputm\0", token, matches);
}

fn split_path_token(sh: &ShellState, token: &[u8], out_dir: &mut [u8], out_prefix: &mut [u8]) -> bool {
    clear_buf(out_dir);
    clear_buf(out_prefix);
    let token_len = c_len(token);
    let mut slash_pos: isize = -1;
    let mut i = 0usize;
    while i < token_len {
        if token[i] == b'/' {
            slash_pos = i as isize;
        }
        i += 1;
    }
    if slash_pos < 0 {
        let _ = copy_bytes(out_dir, &sh.cwd[..c_len(&sh.cwd)]);
        let _ = copy_bytes(out_prefix, &token[..token_len]);
        return true;
    }
    let slash = slash_pos as usize;
    if slash == 0 {
        let _ = copy_bytes(out_dir, b"/");
    } else {
        let mut parent = [0u8; PATH_MAX];
        let _ = copy_bytes(&mut parent, &token[..slash]);
        if !resolve_path(&sh.cwd, &parent, out_dir) {
            return false;
        }
    }
    if slash + 1 <= token_len {
        let _ = copy_bytes(out_prefix, &token[slash + 1..token_len]);
    }
    true
}

fn complete_path(sh: &ShellState, token: &[u8], matches: &mut MatchList) {
    let mut dir = [0u8; PATH_MAX];
    let mut prefix = [0u8; PATH_MAX];
    if !split_path_token(sh, token, &mut dir, &mut prefix) {
        return;
    }
    if syscall(SYSCALL_FS_STAT_TYPE, dir.as_ptr() as U64, 0, 0) != FS_TYPE_DIR {
        return;
    }
    let count = syscall(SYSCALL_FS_CHILD_COUNT, dir.as_ptr() as U64, 0, 0);
    let token_len = c_len(token);
    let mut base_off = 0usize;
    let mut j = 0usize;
    while j < token_len {
        if token[j] == b'/' {
            base_off = j + 1;
        }
        j += 1;
    }
    let mut i = 0u64;
    while i < count && matches.count < MATCH_MAX {
        let mut name = [0u8; FS_NAME_MAX];
        if syscall(SYSCALL_FS_GET_CHILD_NAME, dir.as_ptr() as U64, i, name.as_mut_ptr() as U64) != 0
            && has_prefix_token(&name, &prefix)
        {
            let mut child_path = [0u8; PATH_MAX];
            let mut completion = [0u8; PATH_MAX];
            if join_path(&dir, &name, &mut child_path) {
                let typ = syscall(SYSCALL_FS_STAT_TYPE, child_path.as_ptr() as U64, 0, 0);
                if token_len > 0 && token[0] == b'/' {
                    let _ = copy_bytes(&mut completion, &child_path[..c_len(&child_path)]);
                } else {
                    let _ = copy_bytes(&mut completion, &token[..base_off]);
                    let _ = append_cstr(&mut completion, &name);
                }
                if typ == FS_TYPE_DIR {
                    let _ = append_bytes(&mut completion, b"/");
                }
                match_add(matches, &completion);
            }
        }
        i += 1;
    }
}

fn find_completion_token(sh: &ShellState, out_start: &mut usize, out_end: &mut usize, out_token: &mut [u8], out_command: &mut bool) -> bool {
    clear_buf(out_token);
    let mut end = sh.cursor;
    if end > sh.line_len {
        end = sh.line_len;
    }
    let mut start = end;
    while start > 0
        && !is_space(sh.line[start - 1])
        && sh.line[start - 1] != b'|'
        && sh.line[start - 1] != b'>'
        && sh.line[start - 1] != b'<'
    {
        start -= 1;
    }
    let mut command_token = true;
    let mut i = 0usize;
    while i < start {
        if sh.line[i] == b'|' || sh.line[i] == b';' {
            command_token = true;
        } else if !is_space(sh.line[i]) && sh.line[i] != b'>' && sh.line[i] != b'<' {
            command_token = false;
        }
        i += 1;
    }
    *out_start = start;
    *out_end = end;
    let _ = copy_bytes(out_token, &sh.line[start..end]);
    *out_command = command_token;
    true
}

fn replace_range_raw(sh: &mut ShellState, start: usize, end: usize, replacement: &[u8]) {
    if start > end || end > sh.line_len {
        return;
    }
    let mut repl_len = c_len(replacement);
    let tail_len = sh.line_len - end;
    if start + repl_len + tail_len >= LINE_MAX {
        if LINE_MAX - 1 > start + tail_len {
            repl_len = LINE_MAX - 1 - start - tail_len;
        } else {
            repl_len = 0;
        }
    }
    let mut i = 0usize;
    while i <= tail_len {
        sh.line[start + repl_len + i] = sh.line[end + i];
        i += 1;
    }
    i = 0;
    while i < repl_len {
        sh.line[start + i] = replacement[i];
        i += 1;
    }
    sh.line_len = start + repl_len + tail_len;
    sh.line[sh.line_len] = 0;
    sh.cursor = start + repl_len;
    selection_clear(sh);
}

fn common_prefix(matches: &MatchList, out: &mut [u8]) {
    clear_buf(out);
    if matches.count == 0 {
        return;
    }
    let _ = copy_bytes(out, &matches.items[0]);
    let mut prefix_len = c_len(out);
    let mut i = 1usize;
    while i < matches.count {
        let mut j = 0usize;
        while j < prefix_len && matches.items[i][j] != 0 && out[j] == matches.items[i][j] {
            j += 1;
        }
        prefix_len = j;
        out[prefix_len] = 0;
        i += 1;
    }
}

fn show_matches(matches: &MatchList) {
    if matches.count == 0 {
        return;
    }
    let mut limit = matches.count;
    if limit > MATCH_DISPLAY_MAX {
        limit = MATCH_DISPLAY_MAX;
    }
    write_bytes(b"\n");
    let mut i = 0usize;
    while i < limit {
        write_cstr(&matches.items[i]);
        if i + 1 < limit {
            write_bytes(b"  ");
        }
        i += 1;
    }
    if matches.count > limit {
        write_bytes(b"  ...");
    }
    write_bytes(b"\n");
}

fn linenoise_complete(sh: &mut ShellState) -> U64 {
    let mut matches = MatchList { items: [[0; PATH_MAX]; MATCH_MAX], count: 0 };
    match_list_clear(&mut matches);
    let mut token = [0u8; PATH_MAX];
    let mut common = [0u8; PATH_MAX];
    let mut start = 0usize;
    let mut end = 0usize;
    let mut command_token = false;
    if !find_completion_token(sh, &mut start, &mut end, &mut token, &mut command_token) {
        return COMPLETE_NONE;
    }
    if command_token && !contains_byte(&token, b'/') {
        complete_commands(&token, &mut matches);
        complete_external_commands(&token, &mut matches);
    }
    complete_path(sh, &token, &mut matches);
    if matches.count == 0 {
        write_bytes(b"\x07");
        return COMPLETE_NONE;
    }
    if matches.count == 1 {
        replace_range_raw(sh, start, end, &matches.items[0]);
        let item_len = c_len(&matches.items[0]);
        if item_len > 0 && matches.items[0][item_len - 1] != b'/' && sh.line_len + 1 < LINE_MAX {
            replace_range_raw(sh, sh.cursor, sh.cursor, b" ");
        }
        render_line(sh);
        return COMPLETE_EDITED;
    }
    common_prefix(&matches, &mut common);
    if c_len(&common) > c_len(&token) {
        replace_range_raw(sh, start, end, &common);
        render_line(sh);
        return COMPLETE_EDITED;
    }
    show_matches(&matches);
    render_line(sh);
    COMPLETE_LISTED
}

fn linenoise_reverse_search(sh: &mut ShellState) {
    let mut query = [0u8; LINE_MAX];
    let mut query_len = 0usize;
    write_bytes(b"\n(reverse-i-search)`': ");
    loop {
        let ch = read_char_blocking();
        let mut match_idx: isize = -1;
        if ch == b'\r' {
            continue;
        }
        if ch == b'\n' {
            write_bytes(b"\n");
            if query_len > 0 {
                let mut i = sh.history_count as isize - 1;
                while i >= 0 {
                    if contains_substr(&sh.history[i as usize], &query) {
                        match_idx = i;
                        break;
                    }
                    i -= 1;
                }
            }
            if match_idx >= 0 {
                let line = sh.history[match_idx as usize];
                load_line(sh, &line);
            }
            render_line(sh);
            return;
        }
        if ch == 27 || ch == KEY_REVERSE_SEARCH {
            write_bytes(b"\n");
            render_line(sh);
            return;
        }
        if ch == 8 || ch == 127 || ch == KEY_DELETE {
            if query_len > 0 {
                query_len -= 1;
                query[query_len] = 0;
            }
        } else if is_printable(ch) && query_len + 1 < query.len() {
            query[query_len] = ch;
            query_len += 1;
            query[query_len] = 0;
        } else {
            continue;
        }
        let mut i = sh.history_count as isize - 1;
        while i >= 0 {
            if contains_substr(&sh.history[i as usize], &query) {
                match_idx = i;
                break;
            }
            i -= 1;
        }
        write_bytes(b"\r(reverse-i-search)`");
        write_cstr(&query);
        write_bytes(b"': ");
        if match_idx >= 0 {
            write_cstr(&sh.history[match_idx as usize]);
        }
        write_bytes(b"          ");
    }
}

fn read_interactive_line(sh: &mut ShellState, out: &mut [u8]) {
    reset_line(sh);
    history_cancel_nav(sh);
    clear_buf(out);
    write_prompt(sh);
    loop {
        let ch = read_char_blocking();
        if ch == b'\r' {
            continue;
        }
        if ch == b'\n' {
            if line_needs_continuation(&sh.line, sh.line_len) {
                history_cancel_nav(sh);
                selection_clear(sh);
                remove_continuation_marker(sh);
                if sh.line_len + 1 < LINE_MAX {
                    insert_text(sh, b" ");
                }
                write_bytes(b"\n... ");
                sh.rendered_len = 0;
                continue;
            }
            write_bytes(b"\n");
            sh.line[sh.line_len] = 0;
            let current_line = sh.line;
            history_push(sh, &current_line);
            let _ = copy_bytes(out, &sh.line[..sh.line_len]);
            reset_line(sh);
            return;
        }
        if ch == KEY_SELECT_ALL {
            selection_select_all(sh);
            sh.cursor = sh.line_len;
            render_line(sh);
            continue;
        }
        if ch == KEY_COPY {
            copy_selection(sh);
            continue;
        }
        if ch == KEY_PASTE {
            if sh.clipboard_len > 0 {
                history_cancel_nav(sh);
                let _ = delete_selection(sh);
                let clip = sh.clipboard;
                insert_text(sh, &clip[..sh.clipboard_len]);
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_SHIFT_LEFT || ch == KEY_SHIFT_RIGHT || ch == KEY_SHIFT_HOME || ch == KEY_SHIFT_END {
            if !sh.sel_anchor_valid {
                sh.sel_anchor = sh.cursor;
                sh.sel_anchor_valid = true;
            }
            if ch == KEY_SHIFT_LEFT && sh.cursor > 0 {
                sh.cursor = utf8_prev_boundary(&sh.line, sh.line_len, sh.cursor);
            } else if ch == KEY_SHIFT_RIGHT && sh.cursor < sh.line_len {
                sh.cursor = utf8_next_boundary(&sh.line, sh.line_len, sh.cursor);
            } else if ch == KEY_SHIFT_HOME {
                sh.cursor = 0;
            } else if ch == KEY_SHIFT_END {
                sh.cursor = sh.line_len;
            }
            selection_update_from_anchor(sh);
            render_line(sh);
            continue;
        }
        if ch == KEY_LINE_START || ch == KEY_HOME {
            selection_clear(sh);
            if sh.cursor != 0 {
                sh.cursor = 0;
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_LINE_END || ch == KEY_END {
            selection_clear(sh);
            if sh.cursor != sh.line_len {
                sh.cursor = sh.line_len;
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_KILL_BEFORE {
            if sh.cursor > 0 {
                history_cancel_nav(sh);
                cut_range_to_clipboard(sh, 0, sh.cursor);
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_KILL_AFTER {
            if sh.cursor < sh.line_len {
                history_cancel_nav(sh);
                cut_range_to_clipboard(sh, sh.cursor, sh.line_len);
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_KILL_WORD_BEFORE {
            if sh.cursor > 0 {
                let start = prev_word_boundary(&sh.line, sh.line_len, sh.cursor);
                if start < sh.cursor {
                    history_cancel_nav(sh);
                    cut_range_to_clipboard(sh, start, sh.cursor);
                    render_line(sh);
                }
            }
            continue;
        }
        if ch == KEY_CLEAR_SCREEN {
            write_bytes(b"\x1B[2J\x1B[3J\x1B[H");
            sh.rendered_len = 0;
            render_line(sh);
            continue;
        }
        if ch == KEY_EOF_OR_DELETE {
            if sh.line_len == 0 {
                write_bytes(b"exit\n");
                let _ = copy_bytes(out, b"exit");
                reset_line(sh);
                return;
            }
            if sh.cursor < sh.line_len {
                let next = utf8_next_boundary(&sh.line, sh.line_len, sh.cursor);
                history_cancel_nav(sh);
                delete_range(sh, sh.cursor, next);
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_REVERSE_SEARCH {
            linenoise_reverse_search(sh);
            continue;
        }
        if ch == KEY_UP {
            selection_clear(sh);
            history_up(sh);
            continue;
        }
        if ch == KEY_DOWN {
            selection_clear(sh);
            history_down(sh);
            continue;
        }
        if ch == KEY_LEFT {
            selection_clear(sh);
            if sh.cursor > 0 {
                sh.cursor = utf8_prev_boundary(&sh.line, sh.line_len, sh.cursor);
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_RIGHT {
            selection_clear(sh);
            if sh.cursor < sh.line_len {
                sh.cursor = utf8_next_boundary(&sh.line, sh.line_len, sh.cursor);
                render_line(sh);
            }
            continue;
        }
        if ch == b'\t' {
            let rc = linenoise_complete(sh);
            if rc == COMPLETE_EDITED || rc == COMPLETE_LISTED {
                history_cancel_nav(sh);
            }
            continue;
        }
        if ch == 8 || ch == 127 {
            if delete_selection(sh) {
                history_cancel_nav(sh);
                render_line(sh);
                continue;
            }
            if sh.cursor > 0 && sh.line_len > 0 {
                let start = utf8_prev_boundary(&sh.line, sh.line_len, sh.cursor);
                history_cancel_nav(sh);
                selection_clear(sh);
                delete_range(sh, start, sh.cursor);
                render_line(sh);
            }
            continue;
        }
        if ch == KEY_DELETE {
            if delete_selection(sh) {
                history_cancel_nav(sh);
                render_line(sh);
                continue;
            }
            if sh.cursor < sh.line_len {
                let next = utf8_next_boundary(&sh.line, sh.line_len, sh.cursor);
                history_cancel_nav(sh);
                selection_clear(sh);
                delete_range(sh, sh.cursor, next);
                render_line(sh);
            }
            continue;
        }
        if !is_printable(ch) {
            continue;
        }
        history_cancel_nav(sh);
        let _ = delete_selection(sh);
        if sh.line_len + 1 >= LINE_MAX {
            continue;
        }
        let mut insert_buf = [0u8; 4];
        let mut insert_len = 1usize;
        insert_buf[0] = ch;
        if ch >= 0x80 {
            insert_len = collect_utf8(ch, &mut insert_buf);
        }
        if sh.line_len + insert_len >= LINE_MAX {
            continue;
        }
        insert_text(sh, &insert_buf[..insert_len]);
        render_line(sh);
    }
}

fn command_ret_reset() {
    let _ = syscall(SYSCALL_FS_REMOVE, CMD_RET_PATH.as_ptr() as U64, 0, 0);
}

fn command_ret_read(out: &mut CmdRet) -> bool {
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

fn command_ctx_write(cmd: &[u8], arg: &[u8], cwd: &[u8]) -> bool {
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

fn signal_name(signal: U64) -> &'static [u8] {
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

fn print_exit_status(label: &[u8], status: U64) {
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

fn starts_with_raw(text: &[u8], prefix: &[u8]) -> bool {
    let len = c_len(text);
    len >= prefix.len() && text[..prefix.len()] == *prefix
}

fn alias_command(cmd: &[u8], out: &mut [u8]) {
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

fn build_env(sh: &ShellState, cmd: &[u8], stdin_fd: U64, env: &mut [u8]) -> bool {
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

fn apply_command_ret(sh: &mut ShellState, ret: &CmdRet) {
    if (ret.flags & CMD_RET_FLAG_CWD) != 0 && ret.cwd[0] == b'/' {
        let _ = copy_bytes(&mut sh.cwd, &ret.cwd[..c_len(&ret.cwd)]);
    }
    if (ret.flags & CMD_RET_FLAG_EXIT) != 0 {
        sh.exit_requested = true;
        sh.exit_code = ret.exit_code;
    }
}

fn sync_user_state_after_external(sh: &mut ShellState, stdin_fd: U64, stdout_fd: U64, stderr_fd: U64) {
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

fn exec_external_with_fds(
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

fn exec_external(sh: &mut ShellState, cmd: &[u8], arg: &[u8]) -> ExecResult {
    exec_external_with_fds(sh, cmd, arg, FD_INHERIT, FD_INHERIT, FD_INHERIT)
}

fn builtin_cd(sh: &mut ShellState, arg: &[u8]) -> bool {
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

fn print_help() {
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

fn builtin_shstat(sh: &ShellState) {
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

fn run_script_file(sh: &mut ShellState, path: &[u8]) -> bool {
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

fn builtin_source(sh: &mut ShellState, arg: &[u8]) -> bool {
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

fn run_builtin(sh: &mut ShellState, cmd: &[u8], arg: &[u8]) -> Option<bool> {
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

fn pipeline_has_meta(line: &[u8]) -> bool {
    let len = c_len(line);
    let mut i = 0usize;
    while i < len {
        if line[i] == b'|' || line[i] == b'>' {
            return true;
        }
        i += 1;
    }
    false
}

fn parse_pipeline_stage(stage: &mut PipelineStage, segment_text: &[u8]) -> bool {
    stage.text = [0; LINE_MAX];
    stage.cmd = [0; 32];
    stage.arg = [0; ARG_MAX];
    stage.redirect_path = [0; PATH_MAX];
    stage.redirect_mode = 0;

    let mut work = [0u8; LINE_MAX];
    let _ = copy_bytes(&mut work, &segment_text[..c_len(segment_text)]);
    trim_ascii_in_place(&mut work);
    if c_len(&work) == 0 {
        writeln(b"pipe: empty command stage");
        return false;
    }

    let work_len = c_len(&work);
    let mut op_pos: isize = -1;
    let mut op_mode = 0u8;
    let mut i = 0usize;
    while i < work_len {
        if work[i] == b'>' {
            if op_pos >= 0 {
                writeln(b"pipe: multiple redirections in one stage are not supported");
                return false;
            }
            op_pos = i as isize;
            if i + 1 < work_len && work[i + 1] == b'>' {
                op_mode = 2;
                i += 1;
            } else {
                op_mode = 1;
            }
        }
        i += 1;
    }

    if op_pos >= 0 {
        let op = op_pos as usize;
        let path_start = op + if op_mode == 2 { 2 } else { 1 };
        let mut path_src = [0u8; PATH_MAX];
        if path_start <= work_len {
            let _ = copy_bytes(&mut path_src, &work[path_start..work_len]);
        }
        work[op] = 0;
        trim_ascii_in_place(&mut work);
        trim_ascii_in_place(&mut path_src);
        if c_len(&path_src) == 0 {
            writeln(b"pipe: redirection path required");
            return false;
        }
        let mut first = [0u8; PATH_MAX];
        let mut rest = [0u8; ARG_MAX];
        if !split_first_and_rest(&path_src, &mut first, &mut rest) {
            writeln(b"pipe: redirection path required");
            return false;
        }
        if c_len(&rest) != 0 {
            writeln(b"pipe: redirection path cannot contain spaces");
            return false;
        }
        stage.redirect_mode = op_mode;
        let _ = copy_bytes(&mut stage.redirect_path, &first[..c_len(&first)]);
    }

    let _ = copy_bytes(&mut stage.text, &work[..c_len(&work)]);
    split_line(&work, &mut stage.cmd, &mut stage.arg);
    trim_ascii_in_place(&mut stage.arg);
    if c_len(&stage.cmd) == 0 {
        writeln(b"pipe: empty command stage");
        return false;
    }
    true
}

fn parse_pipeline(line: &[u8], stages: &mut [PipelineStage; PIPELINE_MAX_STAGES], out_count: &mut usize) -> bool {
    *out_count = 0;
    let len = c_len(line);
    let mut segment = [0u8; LINE_MAX];
    let mut seg_pos = 0usize;
    let mut i = 0usize;
    loop {
        let ch = if i < len { line[i] } else { 0 };
        if ch == b'|' || ch == 0 {
            segment[seg_pos] = 0;
            if *out_count >= PIPELINE_MAX_STAGES {
                writeln(b"pipe: too many stages");
                return false;
            }
            if !parse_pipeline_stage(&mut stages[*out_count], &segment) {
                return false;
            }
            *out_count += 1;
            seg_pos = 0;
            clear_buf(&mut segment);
            if ch == 0 {
                break;
            }
            i += 1;
            continue;
        }
        if seg_pos + 1 >= LINE_MAX {
            writeln(b"pipe: stage text too long");
            return false;
        }
        segment[seg_pos] = ch;
        seg_pos += 1;
        i += 1;
    }
    true
}

fn fd_open_path(path: &[u8], flags: U64) -> U64 {
    syscall(SYSCALL_FD_OPEN, path.as_ptr() as U64, flags, 0)
}

fn execute_pipeline(sh: &mut ShellState, line: &[u8]) -> ExecResult {
    let empty = PipelineStage {
        text: [0; LINE_MAX],
        cmd: [0; 32],
        arg: [0; ARG_MAX],
        redirect_path: [0; PATH_MAX],
        redirect_mode: 0,
    };
    let mut stages = [empty; PIPELINE_MAX_STAGES];
    let mut stage_count = 0usize;
    let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_A.as_ptr() as U64, 0, 0);
    let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_B.as_ptr() as U64, 0, 0);
    if !parse_pipeline(line, &mut stages, &mut stage_count) {
        return ExecResult { known: true, success: false };
    }

    let mut pipe_input_path: Option<&'static [u8]> = None;
    let mut toggle = 0u8;
    let mut i = 0usize;
    while i < stage_count {
        if i + 1 < stage_count && stages[i].redirect_mode != 0 {
            writeln(b"pipe: redirection is only supported on final stage");
            return ExecResult { known: true, success: false };
        }

        let mut stdin_fd = FD_INHERIT;
        let mut stdout_fd = FD_INHERIT;
        let mut opened_in = !0u64;
        let mut opened_out = !0u64;
        let mut stage_pipe_out: Option<&'static [u8]> = None;

        if let Some(input_path) = pipe_input_path {
            opened_in = fd_open_path(input_path, O_RDONLY);
            if opened_in == !0 {
                writeln(b"pipe: failed to open stage input");
                return ExecResult { known: true, success: false };
            }
            stdin_fd = opened_in;
        }

        if i + 1 < stage_count {
            let out_path = if toggle == 0 { PIPE_TMP_A } else { PIPE_TMP_B };
            opened_out = fd_open_path(out_path, O_WRONLY | O_CREAT | O_TRUNC);
            if opened_out == !0 {
                if opened_in != !0 {
                    let _ = syscall(SYSCALL_FD_CLOSE, opened_in, 0, 0);
                }
                writeln(b"pipe: failed to open temp stream");
                return ExecResult { known: true, success: false };
            }
            stdout_fd = opened_out;
            stage_pipe_out = Some(out_path);
        } else if stages[i].redirect_mode != 0 {
            let mut abs_path = [0u8; PATH_MAX];
            if !resolve_path(&sh.cwd, &stages[i].redirect_path, &mut abs_path) {
                if opened_in != !0 {
                    let _ = syscall(SYSCALL_FD_CLOSE, opened_in, 0, 0);
                }
                writeln(b"redirect: invalid path");
                return ExecResult { known: true, success: false };
            }
            let mut flags = O_WRONLY | O_CREAT;
            if stages[i].redirect_mode == 1 {
                flags |= O_TRUNC;
            } else {
                flags |= O_APPEND;
            }
            opened_out = fd_open_path(&abs_path, flags);
            if opened_out == !0 {
                if opened_in != !0 {
                    let _ = syscall(SYSCALL_FD_CLOSE, opened_in, 0, 0);
                }
                writeln(b"redirect: open failed");
                return ExecResult { known: true, success: false };
            }
            stdout_fd = opened_out;
        }

        let result = exec_external_with_fds(sh, &stages[i].cmd, &stages[i].arg, stdin_fd, stdout_fd, FD_INHERIT);
        if opened_in != !0 {
            let _ = syscall(SYSCALL_FD_CLOSE, opened_in, 0, 0);
        }
        if opened_out != !0 {
            let _ = syscall(SYSCALL_FD_CLOSE, opened_out, 0, 0);
        }

        if !result.known {
            write_bytes(b"command not found (external ELF required): ");
            write_cstr(&stages[i].cmd);
            write_bytes(b"\n");
            let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_A.as_ptr() as U64, 0, 0);
            let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_B.as_ptr() as U64, 0, 0);
            return ExecResult { known: false, success: false };
        }
        if !result.success {
            let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_A.as_ptr() as U64, 0, 0);
            let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_B.as_ptr() as U64, 0, 0);
            return ExecResult { known: true, success: false };
        }

        if i + 1 < stage_count {
            pipe_input_path = stage_pipe_out;
            toggle = if toggle == 0 { 1 } else { 0 };
        }
        i += 1;
    }

    let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_A.as_ptr() as U64, 0, 0);
    let _ = syscall(SYSCALL_FS_REMOVE, PIPE_TMP_B.as_ptr() as U64, 0, 0);
    ExecResult { known: true, success: true }
}

fn execute_line(sh: &mut ShellState, line: &[u8]) {
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

fn argv_copy(argv: *mut *mut u8, index: isize, out: &mut [u8]) -> bool {
    clear_buf(out);
    if argv.is_null() || index < 0 {
        return false;
    }
    unsafe {
        let ptr_item = *argv.offset(index);
        copy_from_ptr(out, ptr_item as *const u8)
    }
}

fn run_startup_arguments(sh: &mut ShellState, argc: i32, argv: *mut *mut u8) {
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

fn shell_main_impl(argc: i32, argv: *mut *mut u8) -> i32 {
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
