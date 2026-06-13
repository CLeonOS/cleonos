use super::*;

pub(crate) fn pipeline_has_meta(line: &[u8]) -> bool {
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

pub(crate) fn parse_pipeline_stage(stage: &mut PipelineStage, segment_text: &[u8]) -> bool {
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

pub(crate) fn parse_pipeline(line: &[u8], stages: &mut [PipelineStage; PIPELINE_MAX_STAGES], out_count: &mut usize) -> bool {
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

pub(crate) fn fd_open_path(path: &[u8], flags: U64) -> U64 {
    syscall(SYSCALL_FD_OPEN, path.as_ptr() as U64, flags, 0)
}

pub(crate) fn execute_pipeline(sh: &mut ShellState, line: &[u8]) -> ExecResult {
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
