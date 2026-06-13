use super::*;

pub(crate) fn join_path(dir_path: &[u8], name: &[u8], out: &mut [u8]) -> bool {
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

pub(crate) fn has_prefix_token(text: &[u8], prefix: &[u8]) -> bool {
    let text_len = c_len(text);
    let prefix_len = c_len(prefix);
    text_len >= prefix_len && text[..prefix_len] == prefix[..prefix_len]
}

pub(crate) fn contains_substr(text: &[u8], needle: &[u8]) -> bool {
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

pub(crate) fn match_list_clear(matches: &mut MatchList) {
    matches.count = 0;
    let mut i = 0usize;
    while i < MATCH_MAX {
        clear_buf(&mut matches.items[i]);
        i += 1;
    }
}

pub(crate) fn match_add(matches: &mut MatchList, text: &[u8]) {
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

pub(crate) fn complete_commands(token: &[u8], matches: &mut MatchList) {
    for command in COMMANDS.iter() {
        if command.len() >= c_len(token) && command[..c_len(token)] == token[..c_len(token)] {
            match_add(matches, command);
        }
    }
}

pub(crate) fn complete_elf_dir(dir: &[u8], token: &[u8], matches: &mut MatchList) {
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

pub(crate) fn complete_external_commands(token: &[u8], matches: &mut MatchList) {
    complete_elf_dir(b"/shell/apps\0", token, matches);
    complete_elf_dir(b"/shell/apps/uwm\0", token, matches);
    complete_elf_dir(b"/shell/apps/inputm\0", token, matches);
}

pub(crate) fn split_path_token(sh: &ShellState, token: &[u8], out_dir: &mut [u8], out_prefix: &mut [u8]) -> bool {
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

pub(crate) fn complete_path(sh: &ShellState, token: &[u8], matches: &mut MatchList) {
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

pub(crate) fn find_completion_token(sh: &ShellState, out_start: &mut usize, out_end: &mut usize, out_token: &mut [u8], out_command: &mut bool) -> bool {
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

pub(crate) fn replace_range_raw(sh: &mut ShellState, start: usize, end: usize, replacement: &[u8]) {
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

pub(crate) fn common_prefix(matches: &MatchList, out: &mut [u8]) {
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

pub(crate) fn show_matches(matches: &MatchList) {
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

pub(crate) fn linenoise_complete(sh: &mut ShellState) -> U64 {
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

pub(crate) fn linenoise_reverse_search(sh: &mut ShellState) {
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

