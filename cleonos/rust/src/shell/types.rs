use super::*;

#[repr(C)]
pub(crate) struct UserInfo {
    pub(crate) uid: U64,
    pub(crate) role: U64,
    pub(crate) logged_in: U64,
    pub(crate) disk_login_required: U64,
    pub(crate) name: [u8; NAME_MAX],
    pub(crate) home: [u8; HOME_MAX],
}

#[repr(C)]
pub(crate) struct UserLoginReq {
    pub(crate) name_ptr: U64,
    pub(crate) password_ptr: U64,
    pub(crate) out_info_ptr: U64,
}

#[repr(C)]
pub(crate) struct CmdCtx {
    pub(crate) cmd: [u8; 32],
    pub(crate) arg: [u8; ARG_MAX],
    pub(crate) cwd: [u8; PATH_MAX],
}

#[repr(C)]
pub(crate) struct CmdRet {
    pub(crate) flags: U64,
    pub(crate) exit_code: U64,
    pub(crate) cwd: [u8; PATH_MAX],
}

#[repr(C)]
pub(crate) struct ExecPathvIoReq {
    pub(crate) env_line_ptr: U64,
    pub(crate) stdin_fd: U64,
    pub(crate) stdout_fd: U64,
    pub(crate) stderr_fd: U64,
}

pub(crate) struct ShellState {
    pub(crate) line: [u8; LINE_MAX],
    pub(crate) line_len: usize,
    pub(crate) cursor: usize,
    pub(crate) rendered_len: usize,

    pub(crate) cwd: [u8; PATH_MAX],
    pub(crate) username: [u8; NAME_MAX],
    pub(crate) home: [u8; HOME_MAX],
    pub(crate) role: U64,
    pub(crate) disk_login_required: bool,
    pub(crate) logged_in: bool,

    pub(crate) history: [[u8; LINE_MAX]; HISTORY_MAX],
    pub(crate) history_count: usize,
    pub(crate) history_nav: i64,
    pub(crate) nav_saved_line: [u8; LINE_MAX],
    pub(crate) nav_saved_len: usize,
    pub(crate) nav_saved_cursor: usize,

    pub(crate) clipboard: [u8; LINE_MAX],
    pub(crate) clipboard_len: usize,
    pub(crate) sel_start: usize,
    pub(crate) sel_end: usize,
    pub(crate) sel_active: bool,
    pub(crate) sel_anchor: usize,
    pub(crate) sel_anchor_valid: bool,

    pub(crate) cmd_total: U64,
    pub(crate) cmd_ok: U64,
    pub(crate) cmd_fail: U64,
    pub(crate) cmd_unknown: U64,
    pub(crate) exit_requested: bool,
    pub(crate) exit_code: U64,
}

#[derive(Clone, Copy)]
pub(crate) struct ExecResult {
    pub(crate) known: bool,
    pub(crate) success: bool,
}

#[derive(Clone, Copy)]
pub(crate) struct PipelineStage {
    pub(crate) text: [u8; LINE_MAX],
    pub(crate) cmd: [u8; 32],
    pub(crate) arg: [u8; ARG_MAX],
    pub(crate) redirect_path: [u8; PATH_MAX],
    pub(crate) redirect_mode: u8,
}

pub(crate) struct MatchList {
    pub(crate) items: [[u8; PATH_MAX]; MATCH_MAX],
    pub(crate) count: usize,
}
