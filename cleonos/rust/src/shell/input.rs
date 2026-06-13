use super::*;

pub(crate) fn read_interactive_line(sh: &mut ShellState, out: &mut [u8]) {
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
