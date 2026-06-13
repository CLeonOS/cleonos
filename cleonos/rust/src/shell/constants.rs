pub(crate) type U64 = u64;

pub(crate) const SYSCALL_FS_READ: U64 = 12;
pub(crate) const SYSCALL_FS_CHILD_COUNT: U64 = 10;
pub(crate) const SYSCALL_FS_GET_CHILD_NAME: U64 = 11;
pub(crate) const SYSCALL_EXEC_PATHV_IO: U64 = 80;
pub(crate) const SYSCALL_TTY_WRITE: U64 = 24;
pub(crate) const SYSCALL_TTY_WRITE_CHAR: U64 = 25;
pub(crate) const SYSCALL_FS_STAT_TYPE: U64 = 27;
pub(crate) const SYSCALL_FS_WRITE: U64 = 30;
pub(crate) const SYSCALL_FS_REMOVE: U64 = 32;
pub(crate) const SYSCALL_YIELD: U64 = 45;
pub(crate) const SYSCALL_FD_OPEN: U64 = 72;
pub(crate) const SYSCALL_FD_READ: U64 = 73;
pub(crate) const SYSCALL_FD_WRITE: U64 = 74;
pub(crate) const SYSCALL_FD_CLOSE: U64 = 75;
pub(crate) const SYSCALL_SLEEP_MS: U64 = 128;
pub(crate) const SYSCALL_PROC_ENVC: U64 = 55;
pub(crate) const SYSCALL_PROC_ENV: U64 = 56;
pub(crate) const SYSCALL_USER_SHELL_READY: U64 = 16;
pub(crate) const SYSCALL_USER_CURRENT: U64 = 132;
pub(crate) const SYSCALL_USER_LOGIN: U64 = 133;
pub(crate) const SYSCALL_USER_LOGOUT: U64 = 134;
pub(crate) const SYSCALL_LOCALE_GET: U64 = 144;

pub(crate) const PATH_MAX: usize = 192;
pub(crate) const LINE_MAX: usize = 192;
pub(crate) const ARG_MAX: usize = 160;
pub(crate) const ENV_MAX: usize = 512;
pub(crate) const NAME_MAX: usize = 32;
pub(crate) const HOME_MAX: usize = 96;
pub(crate) const FS_NAME_MAX: usize = 96;
pub(crate) const HISTORY_MAX: usize = 16;
pub(crate) const HISTORY_DATA_MAX: usize = 4096;
pub(crate) const SCRIPT_MAX: usize = 1024;
pub(crate) const MATCH_MAX: usize = 48;
pub(crate) const MATCH_DISPLAY_MAX: usize = 8;
pub(crate) const PIPELINE_MAX_STAGES: usize = 8;
pub(crate) const CMD_CTX_PATH: &[u8] = b"/temp/.ush_cmd_ctx.bin\0";
pub(crate) const CMD_RET_PATH: &[u8] = b"/temp/.ush_cmd_ret.bin\0";
pub(crate) const HISTORY_DIR: &[u8] = b"/shell/data\0";
pub(crate) const HISTORY_PATH: &[u8] = b"/shell/data/history.txt\0";
pub(crate) const HISTORY_FALLBACK_PATH: &[u8] = b"/temp/shell_history.txt\0";
pub(crate) const PIPE_TMP_A: &[u8] = b"/temp/.ush_pipe_a.bin\0";
pub(crate) const PIPE_TMP_B: &[u8] = b"/temp/.ush_pipe_b.bin\0";
pub(crate) const FD_INHERIT: U64 = !0;
pub(crate) const FD_STDIN: U64 = 0;
pub(crate) const FD_STDOUT: U64 = 1;
pub(crate) const O_RDONLY: U64 = 0x0000;
pub(crate) const O_WRONLY: U64 = 0x0001;
pub(crate) const O_CREAT: U64 = 0x0040;
pub(crate) const O_TRUNC: U64 = 0x0200;
pub(crate) const O_APPEND: U64 = 0x0400;
pub(crate) const USER_ROLE_ADMIN: U64 = 1;
pub(crate) const CMD_RET_FLAG_CWD: U64 = 0x1;
pub(crate) const CMD_RET_FLAG_EXIT: U64 = 0x2;
pub(crate) const FS_TYPE_FILE: U64 = 1;
pub(crate) const FS_TYPE_DIR: U64 = 2;

pub(crate) const KEY_LEFT: u8 = 0x01;
pub(crate) const KEY_RIGHT: u8 = 0x02;
pub(crate) const KEY_UP: u8 = 0x03;
pub(crate) const KEY_DOWN: u8 = 0x04;
pub(crate) const KEY_HOME: u8 = 0x05;
pub(crate) const KEY_END: u8 = 0x06;
pub(crate) const KEY_DELETE: u8 = 0x07;
pub(crate) const KEY_SELECT_ALL: u8 = 0x10;
pub(crate) const KEY_COPY: u8 = 0x11;
pub(crate) const KEY_PASTE: u8 = 0x12;
pub(crate) const KEY_SHIFT_LEFT: u8 = 0x13;
pub(crate) const KEY_SHIFT_RIGHT: u8 = 0x14;
pub(crate) const KEY_SHIFT_HOME: u8 = 0x15;
pub(crate) const KEY_SHIFT_END: u8 = 0x16;
pub(crate) const KEY_REVERSE_SEARCH: u8 = 0x17;
pub(crate) const KEY_LINE_START: u8 = 0x18;
pub(crate) const KEY_LINE_END: u8 = 0x19;
pub(crate) const KEY_KILL_BEFORE: u8 = 0x1A;
pub(crate) const KEY_KILL_AFTER: u8 = 0x1C;
pub(crate) const KEY_KILL_WORD_BEFORE: u8 = 0x1D;
pub(crate) const KEY_CLEAR_SCREEN: u8 = 0x1E;
pub(crate) const KEY_EOF_OR_DELETE: u8 = 0x1F;
pub(crate) const COMPLETE_NONE: U64 = 0;
pub(crate) const COMPLETE_EDITED: U64 = 1;
pub(crate) const COMPLETE_LISTED: U64 = 2;

pub(crate) const COMMANDS: &[&[u8]] = &[
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
