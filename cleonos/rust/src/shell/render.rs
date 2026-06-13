use super::*;

pub(crate) fn write_prompt(sh: &ShellState) {
    let mut render = [0u8; 320];
    let mut len = 0usize;
    render_append_prompt(sh, &mut render, &mut len);
    write_bytes(&render[..len]);
}

pub(crate) fn render_append(out: &mut [u8], len: &mut usize, text: &[u8]) {
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

pub(crate) fn render_append_prompt(sh: &ShellState, out: &mut [u8], len: &mut usize) {
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

pub(crate) fn render_line_segment(sh: &ShellState, limit: usize, out: &mut [u8], out_len: &mut usize) {
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

pub(crate) fn linenoise_hint(sh: &ShellState, hint: &mut [u8]) {
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

pub(crate) fn render_line(sh: &mut ShellState) {
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
