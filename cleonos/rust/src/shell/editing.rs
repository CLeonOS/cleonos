use super::*;

pub(crate) fn reset_line(sh: &mut ShellState) {
    sh.line_len = 0;
    sh.cursor = 0;
    sh.rendered_len = 0;
    clear_buf(&mut sh.line);
    selection_clear(sh);
}

pub(crate) fn insert_text(sh: &mut ShellState, text: &[u8]) {
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

pub(crate) fn delete_range(sh: &mut ShellState, start: usize, mut end: usize) {
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

pub(crate) fn selection_clear(sh: &mut ShellState) {
    sh.sel_active = false;
    sh.sel_start = 0;
    sh.sel_end = 0;
    sh.sel_anchor = 0;
    sh.sel_anchor_valid = false;
}

pub(crate) fn selection_select_all(sh: &mut ShellState) {
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

pub(crate) fn selection_update_from_anchor(sh: &mut ShellState) {
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

pub(crate) fn selection_range(sh: &ShellState, out_start: &mut usize, out_end: &mut usize) -> bool {
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

pub(crate) fn copy_selection(sh: &mut ShellState) {
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

pub(crate) fn delete_selection(sh: &mut ShellState) -> bool {
    let mut start = 0usize;
    let mut end = 0usize;
    if !selection_range(sh, &mut start, &mut end) {
        return false;
    }
    delete_range(sh, start, end);
    true
}

pub(crate) fn cut_range_to_clipboard(sh: &mut ShellState, start: usize, mut end: usize) {
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

pub(crate) fn prev_word_boundary(text: &[u8], len: usize, mut pos: usize) -> usize {
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

pub(crate) fn line_needs_continuation(line: &[u8], len: usize) -> bool {
    if len == 0 {
        return false;
    }
    let mut pos = len;
    while pos > 0 && is_space(line[pos - 1]) {
        pos -= 1;
    }
    pos > 0 && line[pos - 1] == b'\\'
}

pub(crate) fn remove_continuation_marker(sh: &mut ShellState) {
    let mut pos = sh.line_len;
    while pos > 0 && is_space(sh.line[pos - 1]) {
        pos -= 1;
    }
    if pos > 0 && sh.line[pos - 1] == b'\\' {
        delete_range(sh, pos - 1, pos);
        sh.cursor = sh.line_len;
    }
}

