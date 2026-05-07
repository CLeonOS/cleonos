#include "shell_internal.h"

static char ush_input_clipboard[USH_LINE_MAX];
static u64 ush_input_clipboard_len = 0ULL;
static u64 ush_input_sel_start = 0ULL;
static u64 ush_input_sel_end = 0ULL;
static int ush_input_sel_active = 0;
static u64 ush_input_sel_anchor = 0ULL;
static int ush_input_sel_anchor_valid = 0;
#define USH_RENDER_BUF_MAX 4096ULL
#define USH_RENDER_EMIT_CHUNK 2000ULL

static void ush_input_selection_clear(void) {
    ush_input_sel_active = 0;
    ush_input_sel_start = 0ULL;
    ush_input_sel_end = 0ULL;
    ush_input_sel_anchor = 0ULL;
    ush_input_sel_anchor_valid = 0;
}

static void ush_input_selection_select_all(const ush_state *sh) {
    if (sh == (const ush_state *)0 || sh->line_len == 0ULL) {
        ush_input_selection_clear();
        return;
    }

    ush_input_sel_active = 1;
    ush_input_sel_start = 0ULL;
    ush_input_sel_end = sh->line_len;
    ush_input_sel_anchor = 0ULL;
    ush_input_sel_anchor_valid = 1;
}

static void ush_input_selection_update_from_anchor(const ush_state *sh) {
    u64 anchor;

    if (sh == (const ush_state *)0 || ush_input_sel_anchor_valid == 0) {
        ush_input_sel_active = 0;
        return;
    }

    anchor = ush_input_sel_anchor;
    if (anchor > sh->line_len) {
        anchor = sh->line_len;
    }

    if (sh->cursor == anchor) {
        ush_input_sel_active = 0;
        ush_input_sel_start = anchor;
        ush_input_sel_end = anchor;
        return;
    }

    ush_input_sel_active = 1;

    if (sh->cursor < anchor) {
        ush_input_sel_start = sh->cursor;
        ush_input_sel_end = anchor;
    } else {
        ush_input_sel_start = anchor;
        ush_input_sel_end = sh->cursor;
    }
}
static int ush_input_selection_has_range(const ush_state *sh, u64 *out_start, u64 *out_end) {
    u64 start;
    u64 end;

    if (sh == (const ush_state *)0 || ush_input_sel_active == 0) {
        return 0;
    }

    start = ush_input_sel_start;
    end = ush_input_sel_end;

    if (start > sh->line_len) {
        start = sh->line_len;
    }

    if (end > sh->line_len) {
        end = sh->line_len;
    }

    if (start > end) {
        u64 tmp = start;
        start = end;
        end = tmp;
    }

    if (start == end) {
        return 0;
    }

    if (out_start != (u64 *)0) {
        *out_start = start;
    }

    if (out_end != (u64 *)0) {
        *out_end = end;
    }

    return 1;
}

static void ush_input_delete_range(ush_state *sh, u64 start, u64 end) {
    u64 delta;
    u64 i;

    if (sh == (ush_state *)0 || start >= end || start >= sh->line_len) {
        ush_input_selection_clear();
        return;
    }

    if (end > sh->line_len) {
        end = sh->line_len;
    }

    delta = end - start;

    for (i = start; i + delta <= sh->line_len; i++) {
        sh->line[i] = sh->line[i + delta];
    }

    sh->line_len -= delta;

    if (sh->cursor > end) {
        sh->cursor -= delta;
    } else if (sh->cursor > start) {
        sh->cursor = start;
    }

    if (sh->cursor > sh->line_len) {
        sh->cursor = sh->line_len;
    }

    ush_input_selection_clear();
}

static int ush_input_delete_selection(ush_state *sh) {
    u64 start;
    u64 end;

    if (ush_input_selection_has_range(sh, &start, &end) == 0) {
        return 0;
    }

    ush_input_delete_range(sh, start, end);
    return 1;
}

static void ush_input_copy_selection(const ush_state *sh) {
    u64 start;
    u64 end;
    u64 len;
    u64 i;

    if (ush_input_selection_has_range(sh, &start, &end) == 0) {
        return;
    }

    len = end - start;
    if (len > USH_LINE_MAX - 1ULL) {
        len = USH_LINE_MAX - 1ULL;
    }

    for (i = 0ULL; i < len; i++) {
        ush_input_clipboard[i] = sh->line[start + i];
    }

    ush_input_clipboard[len] = '\0';
    ush_input_clipboard_len = len;
}

static void ush_input_insert_text(ush_state *sh, const char *text, u64 text_len) {
    u64 available;
    u64 i;

    if (sh == (ush_state *)0 || text == (const char *)0 || text_len == 0ULL) {
        return;
    }

    if (sh->cursor > sh->line_len) {
        sh->cursor = sh->line_len;
    }

    available = (USH_LINE_MAX - 1ULL) - sh->line_len;
    if (text_len > available) {
        text_len = available;
    }

    if (text_len == 0ULL) {
        return;
    }

    for (i = sh->line_len + 1ULL; i > sh->cursor; i--) {
        sh->line[i + text_len - 1ULL] = sh->line[i - 1ULL];
    }

    for (i = 0ULL; i < text_len; i++) {
        sh->line[sh->cursor + i] = text[i];
    }

    sh->line_len += text_len;
    sh->cursor += text_len;
}

static int ush_input_utf8_is_cont(unsigned char ch) {
    return (ch >= 0x80U && ch <= 0xBFU) ? 1 : 0;
}

static u64 ush_input_utf8_len_from_lead(unsigned char lead) {
    if (lead < 0x80U) {
        return 1ULL;
    }
    if (lead >= 0xC2U && lead <= 0xDFU) {
        return 2ULL;
    }
    if (lead >= 0xE0U && lead <= 0xEFU) {
        return 3ULL;
    }
    if (lead >= 0xF0U && lead <= 0xF4U) {
        return 4ULL;
    }
    return 1ULL;
}

static u64 ush_input_utf8_char_len_at(const char *text, u64 len, u64 pos) {
    u64 need;
    u64 i;

    if (text == (const char *)0 || pos >= len) {
        return 0ULL;
    }

    need = ush_input_utf8_len_from_lead((unsigned char)text[pos]);
    if (need == 1ULL || pos + need > len) {
        return 1ULL;
    }

    for (i = 1ULL; i < need; i++) {
        if (ush_input_utf8_is_cont((unsigned char)text[pos + i]) == 0) {
            return 1ULL;
        }
    }

    return need;
}

static unsigned int ush_input_utf8_decode_at(const char *text, u64 len, u64 pos, u64 *out_adv) {
    unsigned char b0;
    u64 adv;
    unsigned int cp;

    if (out_adv != (u64 *)0) {
        *out_adv = 0ULL;
    }
    if (text == (const char *)0 || pos >= len) {
        return 0U;
    }

    b0 = (unsigned char)text[pos];
    adv = ush_input_utf8_char_len_at(text, len, pos);
    if (out_adv != (u64 *)0) {
        *out_adv = adv;
    }

    if (adv == 1ULL) {
        return (unsigned int)b0;
    }
    if (adv == 2ULL) {
        cp = ((unsigned int)(b0 & 0x1FU) << 6) | (unsigned int)((unsigned char)text[pos + 1ULL] & 0x3FU);
        return cp;
    }
    if (adv == 3ULL) {
        cp = ((unsigned int)(b0 & 0x0FU) << 12) |
             ((unsigned int)((unsigned char)text[pos + 1ULL] & 0x3FU) << 6) |
             (unsigned int)((unsigned char)text[pos + 2ULL] & 0x3FU);
        return cp;
    }

    cp = ((unsigned int)(b0 & 0x07U) << 18) |
         ((unsigned int)((unsigned char)text[pos + 1ULL] & 0x3FU) << 12) |
         ((unsigned int)((unsigned char)text[pos + 2ULL] & 0x3FU) << 6) |
         (unsigned int)((unsigned char)text[pos + 3ULL] & 0x3FU);
    return cp;
}

static u64 ush_input_codepoint_width(unsigned int cp) {
    if (cp == 0U) {
        return 0ULL;
    }
    if ((cp >= 0x0300U && cp <= 0x036FU) || (cp >= 0xFE00U && cp <= 0xFE0FU)) {
        return 0ULL;
    }
    if ((cp >= 0x1100U && cp <= 0x115FU) || (cp >= 0x2E80U && cp <= 0xA4CFU) ||
        (cp >= 0xAC00U && cp <= 0xD7A3U) || (cp >= 0xF900U && cp <= 0xFAFFU) ||
        (cp >= 0xFE10U && cp <= 0xFE19U) || (cp >= 0xFE30U && cp <= 0xFE6FU) ||
        (cp >= 0xFF00U && cp <= 0xFF60U) || (cp >= 0xFFE0U && cp <= 0xFFE6U) ||
        (cp >= 0x20000U && cp <= 0x3FFFDU)) {
        return 2ULL;
    }
    return 1ULL;
}

static u64 ush_input_utf8_visual_width_n(const char *text, u64 len) {
    u64 pos = 0ULL;
    u64 cols = 0ULL;

    while (text != (const char *)0 && pos < len) {
        u64 adv = 1ULL;
        unsigned int cp = ush_input_utf8_decode_at(text, len, pos, &adv);
        if (adv == 0ULL) {
            break;
        }
        cols += ush_input_codepoint_width(cp);
        pos += adv;
    }

    return cols;
}

static u64 ush_input_utf8_prev_boundary(const char *text, u64 len, u64 pos) {
    if (text == (const char *)0 || pos == 0ULL) {
        return 0ULL;
    }
    if (pos > len) {
        pos = len;
    }

    pos--;
    while (pos > 0ULL && ush_input_utf8_is_cont((unsigned char)text[pos]) != 0) {
        pos--;
    }

    return pos;
}

static u64 ush_input_utf8_next_boundary(const char *text, u64 len, u64 pos) {
    u64 adv;

    if (text == (const char *)0 || pos >= len) {
        return len;
    }

    adv = ush_input_utf8_char_len_at(text, len, pos);
    if (adv == 0ULL) {
        adv = 1ULL;
    }
    if (pos + adv > len) {
        return len;
    }

    return pos + adv;
}

static u64 ush_input_collect_utf8(char first, char *out, u64 out_size) {
    u64 need;
    u64 len = 1ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0ULL;
    }

    out[0] = first;
    need = ush_input_utf8_len_from_lead((unsigned char)first);
    if (need > out_size) {
        need = out_size;
    }

    while (len < need) {
        char next = ush_input_read_char_blocking();
        out[len++] = next;
        if (ush_input_utf8_is_cont((unsigned char)next) == 0) {
            break;
        }
    }

    return len;
}

static void ush_history_cancel_nav(ush_state *sh) {
    if (sh == (ush_state *)0) {
        return;
    }

    sh->history_nav = -1;
    sh->nav_saved_len = 0ULL;
    sh->nav_saved_cursor = 0ULL;
    sh->nav_saved_line[0] = '\0';
}

static void ush_reset_line(ush_state *sh) {
    if (sh == (ush_state *)0) {
        return;
    }

    sh->line_len = 0ULL;
    sh->cursor = 0ULL;
    sh->rendered_len = 0ULL;
    sh->line[0] = '\0';
    ush_input_selection_clear();
}

static void ush_load_line(ush_state *sh, const char *line) {
    if (sh == (ush_state *)0) {
        return;
    }

    if (line == (const char *)0) {
        ush_reset_line(sh);
        return;
    }

    ush_copy(sh->line, (u64)sizeof(sh->line), line);
    sh->line_len = ush_strlen(sh->line);
    sh->cursor = sh->line_len;
    ush_input_selection_clear();
}

static void ush_render_buf_append_char(char *out, u64 out_size, u64 *io_len, char ch) {
    if (out == (char *)0 || io_len == (u64 *)0 || out_size == 0ULL) {
        return;
    }

    if (*io_len + 1ULL >= out_size) {
        return;
    }

    out[*io_len] = ch;
    *io_len += 1ULL;
}

static void ush_render_buf_append_text(char *out, u64 out_size, u64 *io_len, const char *text) {
    u64 i = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        ush_render_buf_append_char(out, out_size, io_len, text[i]);
        i++;
    }
}

static void ush_render_buf_append_prompt(const ush_state *sh, char *out, u64 out_size, u64 *io_len) {
    ush_render_buf_append_text(out, out_size, io_len, "\x1B[96mcleonos\x1B[0m(\x1B[92m");

    if (sh == (const ush_state *)0) {
        ush_render_buf_append_text(out, out_size, io_len, "user\x1B[0m)> ");
        return;
    }

    ush_render_buf_append_text(out, out_size, io_len, (sh->username[0] != '\0') ? sh->username : "user");
    ush_render_buf_append_text(out, out_size, io_len, "\x1B[0m:\x1B[93m");
    ush_render_buf_append_text(out, out_size, io_len, sh->cwd);
    ush_render_buf_append_text(out, out_size, io_len, "\x1B[0m)> ");
}

static void ush_render_buf_append_line_segment(const ush_state *sh, u64 limit, char *out, u64 out_size, u64 *io_len) {
    u64 i;
    u64 sel_start = 0ULL;
    u64 sel_end = 0ULL;
    int has_sel;
    int inverse_on = 0;

    if (sh == (const ush_state *)0) {
        return;
    }

    if (limit > sh->line_len) {
        limit = sh->line_len;
    }

    has_sel = ush_input_selection_has_range(sh, &sel_start, &sel_end);

    for (i = 0ULL; i < limit; i++) {
        if (has_sel != 0 && inverse_on == 0 && i == sel_start) {
            ush_render_buf_append_text(out, out_size, io_len, "\x1B[7m");
            inverse_on = 1;
        }

        if (has_sel != 0 && inverse_on != 0 && i == sel_end) {
            ush_render_buf_append_text(out, out_size, io_len, "\x1B[27m");
            inverse_on = 0;
        }

        ush_render_buf_append_char(out, out_size, io_len, sh->line[i]);
    }

    if (inverse_on != 0) {
        ush_render_buf_append_text(out, out_size, io_len, "\x1B[27m");
    }
}

static void ush_render_emit(const char *buffer, u64 length) {
    u64 offset = 0ULL;

    if (buffer == (const char *)0 || length == 0ULL) {
        return;
    }

    while (offset < length) {
        u64 chunk = length - offset;
        u64 written;

        if (chunk > USH_RENDER_EMIT_CHUNK) {
            chunk = USH_RENDER_EMIT_CHUNK;
        }

        written = cleonos_sys_fd_write(1ULL, buffer + offset, chunk);

        if (written == 0ULL || written == (u64)-1) {
            break;
        }

        offset += written;
    }
}

void ush_input_render_line(ush_state *sh) {
    char render[USH_RENDER_BUF_MAX];
    const char *hint;
    u64 hint_visible_len;
    u64 line_cols;
    u64 cursor_cols;
    u64 total_cols;
    u64 out_len = 0ULL;
    u64 i;

    if (sh == (ush_state *)0) {
        return;
    }

    ush_render_buf_append_char(render, (u64)sizeof(render), &out_len, '\r');
    ush_render_buf_append_prompt(sh, render, (u64)sizeof(render), &out_len);
    ush_render_buf_append_line_segment(sh, sh->line_len, render, (u64)sizeof(render), &out_len);
    hint = ush_linenoise_hint(sh);
    hint_visible_len = ush_linenoise_hint_visible_len(hint);
    line_cols = ush_input_utf8_visual_width_n(sh->line, sh->line_len);
    cursor_cols = ush_input_utf8_visual_width_n(sh->line, sh->cursor);
    total_cols = line_cols + hint_visible_len;
    if (hint != (const char *)0 && hint[0] != '\0') {
        ush_render_buf_append_text(render, (u64)sizeof(render), &out_len, "\x1B[90m");
        ush_render_buf_append_text(render, (u64)sizeof(render), &out_len, hint);
        ush_render_buf_append_text(render, (u64)sizeof(render), &out_len, "\x1B[0m");
    }

    for (i = total_cols; i < sh->rendered_len; i++) {
        ush_render_buf_append_char(render, (u64)sizeof(render), &out_len, ' ');
    }

    ush_render_buf_append_char(render, (u64)sizeof(render), &out_len, '\r');
    ush_render_buf_append_prompt(sh, render, (u64)sizeof(render), &out_len);
    ush_render_buf_append_line_segment(sh, sh->cursor, render, (u64)sizeof(render), &out_len);

    ush_render_emit(render, out_len);

    (void)cursor_cols;
    sh->rendered_len = total_cols;
}

static int ush_line_has_non_space(const char *line) {
    u64 i = 0ULL;

    if (line == (const char *)0) {
        return 0;
    }

    while (line[i] != '\0') {
        if (ush_is_space(line[i]) == 0) {
            return 1;
        }
        i++;
    }

    return 0;
}

static void ush_history_push(ush_state *sh, const char *line) {
    if (sh == (ush_state *)0) {
        return;
    }

    if (ush_line_has_non_space(line) == 0) {
        ush_history_cancel_nav(sh);
        return;
    }

    if (sh->history_count > 0ULL && ush_streq(sh->history[sh->history_count - 1ULL], line) != 0) {
        ush_history_cancel_nav(sh);
        return;
    }

    if (sh->history_count < USH_HISTORY_MAX) {
        ush_copy(sh->history[sh->history_count], (u64)sizeof(sh->history[sh->history_count]), line);
        sh->history_count++;
    } else {
        u64 i;

        for (i = 1ULL; i < USH_HISTORY_MAX; i++) {
            ush_copy(sh->history[i - 1ULL], (u64)sizeof(sh->history[i - 1ULL]), sh->history[i]);
        }

        ush_copy(sh->history[USH_HISTORY_MAX - 1ULL], (u64)sizeof(sh->history[USH_HISTORY_MAX - 1ULL]), line);
    }

    ush_history_cancel_nav(sh);
}

static void ush_history_apply_current(ush_state *sh) {
    if (sh == (ush_state *)0) {
        return;
    }

    if (sh->history_nav >= 0) {
        ush_load_line(sh, sh->history[(u64)sh->history_nav]);
    } else {
        ush_copy(sh->line, (u64)sizeof(sh->line), sh->nav_saved_line);
        sh->line_len = sh->nav_saved_len;
        if (sh->line_len > USH_LINE_MAX - 1ULL) {
            sh->line_len = USH_LINE_MAX - 1ULL;
            sh->line[sh->line_len] = '\0';
        }
        sh->cursor = sh->nav_saved_cursor;
        if (sh->cursor > sh->line_len) {
            sh->cursor = sh->line_len;
        }
        ush_input_selection_clear();
    }

    ush_input_render_line(sh);
}

static void ush_history_up(ush_state *sh) {
    if (sh == (ush_state *)0 || sh->history_count == 0ULL) {
        return;
    }

    if (sh->history_nav < 0) {
        ush_copy(sh->nav_saved_line, (u64)sizeof(sh->nav_saved_line), sh->line);
        sh->nav_saved_len = sh->line_len;
        sh->nav_saved_cursor = sh->cursor;
        sh->history_nav = (i64)sh->history_count - 1;
    } else if (sh->history_nav > 0) {
        sh->history_nav--;
    }

    ush_history_apply_current(sh);
}

static void ush_history_down(ush_state *sh) {
    if (sh == (ush_state *)0 || sh->history_nav < 0) {
        return;
    }

    if ((u64)sh->history_nav + 1ULL < sh->history_count) {
        sh->history_nav++;
    } else {
        sh->history_nav = -1;
    }

    ush_history_apply_current(sh);
}

char ush_input_read_char_blocking(void) {
    char ch = '\0';

    for (;;) {
        if (cleonos_sys_fd_read(0ULL, &ch, 1ULL) == 1ULL) {
            return ch;
        }

        __asm__ volatile("pause");
    }
}

void ush_read_plain_line(const char *prompt, char *out_line, u64 out_size) {
    u64 len = 0ULL;

    if (out_line == (char *)0 || out_size == 0ULL) {
        return;
    }

    out_line[0] = '\0';
    if (prompt != (const char *)0) {
        ush_write(prompt);
    }

    for (;;) {
        char ch = ush_input_read_char_blocking();

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            ush_write_char('\n');
            out_line[len] = '\0';
            return;
        }

        if (ch == '\b' || ch == 0x7F || ch == USH_KEY_DELETE) {
            if (len > 0ULL) {
                len--;
                out_line[len] = '\0';
                ush_write("\b \b");
            }
            continue;
        }

        if (ush_is_printable(ch) != 0 && len + 1ULL < out_size) {
            out_line[len++] = ch;
            ush_write_char(ch);
        }
    }
}

void ush_read_secret_line(const char *prompt, char *out_line, u64 out_size) {
    u64 len = 0ULL;

    if (out_line == (char *)0 || out_size == 0ULL) {
        return;
    }

    out_line[0] = '\0';
    if (prompt != (const char *)0) {
        ush_write(prompt);
    }

    for (;;) {
        char ch = ush_input_read_char_blocking();

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            ush_write_char('\n');
            out_line[len] = '\0';
            return;
        }

        if (ch == '\b' || ch == 0x7F || ch == USH_KEY_DELETE) {
            if (len > 0ULL) {
                len--;
                out_line[len] = '\0';
            }
            continue;
        }

        if (ush_is_printable(ch) != 0 && len + 1ULL < out_size) {
            out_line[len++] = ch;
        }
    }
}

void ush_read_line(ush_state *sh, char *out_line, u64 out_size) {
    if (sh == (ush_state *)0 || out_line == (char *)0 || out_size == 0ULL) {
        return;
    }

    ush_reset_line(sh);
    ush_history_cancel_nav(sh);

    out_line[0] = '\0';

    ush_prompt(sh);

    for (;;) {
        char ch = ush_input_read_char_blocking();

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            ush_write_char('\n');
            sh->line[sh->line_len] = '\0';
            ush_history_push(sh, sh->line);
            ush_copy(out_line, out_size, sh->line);
            ush_reset_line(sh);
            return;
        }

        if (ch == USH_KEY_SELECT_ALL) {
            ush_input_selection_select_all(sh);
            sh->cursor = sh->line_len;
            ush_input_render_line(sh);
            continue;
        }

        if (ch == USH_KEY_COPY) {
            ush_input_copy_selection(sh);
            continue;
        }

        if (ch == USH_KEY_PASTE) {
            if (ush_input_clipboard_len > 0ULL) {
                ush_history_cancel_nav(sh);
                (void)ush_input_delete_selection(sh);
                ush_input_insert_text(sh, ush_input_clipboard, ush_input_clipboard_len);
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == USH_KEY_SHIFT_LEFT) {
            if (ush_input_sel_anchor_valid == 0) {
                ush_input_sel_anchor = sh->cursor;
                ush_input_sel_anchor_valid = 1;
            }

            if (sh->cursor > 0ULL) {
                sh->cursor = ush_input_utf8_prev_boundary(sh->line, sh->line_len, sh->cursor);
            }

            ush_input_selection_update_from_anchor(sh);
            ush_input_render_line(sh);
            continue;
        }

        if (ch == USH_KEY_SHIFT_RIGHT) {
            if (ush_input_sel_anchor_valid == 0) {
                ush_input_sel_anchor = sh->cursor;
                ush_input_sel_anchor_valid = 1;
            }

            if (sh->cursor < sh->line_len) {
                sh->cursor = ush_input_utf8_next_boundary(sh->line, sh->line_len, sh->cursor);
            }

            ush_input_selection_update_from_anchor(sh);
            ush_input_render_line(sh);
            continue;
        }

        if (ch == USH_KEY_SHIFT_HOME) {
            if (ush_input_sel_anchor_valid == 0) {
                ush_input_sel_anchor = sh->cursor;
                ush_input_sel_anchor_valid = 1;
            }

            sh->cursor = 0ULL;
            ush_input_selection_update_from_anchor(sh);
            ush_input_render_line(sh);
            continue;
        }

        if (ch == USH_KEY_SHIFT_END) {
            if (ush_input_sel_anchor_valid == 0) {
                ush_input_sel_anchor = sh->cursor;
                ush_input_sel_anchor_valid = 1;
            }

            sh->cursor = sh->line_len;
            ush_input_selection_update_from_anchor(sh);
            ush_input_render_line(sh);
            continue;
        }

        if (ch == USH_KEY_REVERSE_SEARCH) {
            ush_linenoise_reverse_search(sh);
            continue;
        }
        if (ch == USH_KEY_UP) {
            ush_input_selection_clear();
            ush_history_up(sh);
            continue;
        }

        if (ch == USH_KEY_DOWN) {
            ush_input_selection_clear();
            ush_history_down(sh);
            continue;
        }

        if (ch == USH_KEY_LEFT) {
            ush_input_selection_clear();
            if (sh->cursor > 0ULL) {
                sh->cursor = ush_input_utf8_prev_boundary(sh->line, sh->line_len, sh->cursor);
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == USH_KEY_RIGHT) {
            ush_input_selection_clear();
            if (sh->cursor < sh->line_len) {
                sh->cursor = ush_input_utf8_next_boundary(sh->line, sh->line_len, sh->cursor);
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == USH_KEY_HOME) {
            ush_input_selection_clear();
            if (sh->cursor != 0ULL) {
                sh->cursor = 0ULL;
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == USH_KEY_END) {
            ush_input_selection_clear();
            if (sh->cursor != sh->line_len) {
                sh->cursor = sh->line_len;
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == '\b' || ch == 127) {
            if (ush_input_delete_selection(sh) != 0) {
                ush_history_cancel_nav(sh);
                ush_input_render_line(sh);
                continue;
            }

            if (sh->cursor > 0ULL && sh->line_len > 0ULL) {
                u64 delete_start;
                u64 delete_len;
                u64 i;

                ush_history_cancel_nav(sh);
                ush_input_selection_clear();

                delete_start = ush_input_utf8_prev_boundary(sh->line, sh->line_len, sh->cursor);
                delete_len = sh->cursor - delete_start;
                if (delete_len == 0ULL) {
                    delete_len = 1ULL;
                }

                for (i = delete_start; i + delete_len <= sh->line_len; i++) {
                    sh->line[i] = sh->line[i + delete_len];
                }

                sh->line_len -= delete_len;
                sh->cursor = delete_start;
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == USH_KEY_DELETE) {
            if (ush_input_delete_selection(sh) != 0) {
                ush_history_cancel_nav(sh);
                ush_input_render_line(sh);
                continue;
            }

            if (sh->cursor < sh->line_len) {
                u64 next;
                u64 delete_len;
                u64 i;

                ush_history_cancel_nav(sh);
                ush_input_selection_clear();

                next = ush_input_utf8_next_boundary(sh->line, sh->line_len, sh->cursor);
                delete_len = next - sh->cursor;
                if (delete_len == 0ULL) {
                    delete_len = 1ULL;
                }

                for (i = sh->cursor; i + delete_len <= sh->line_len; i++) {
                    sh->line[i] = sh->line[i + delete_len];
                }

                sh->line_len -= delete_len;
                ush_input_render_line(sh);
            }
            continue;
        }

        if (ch == '\t') {
            int complete_rc = ush_linenoise_complete(sh);
            if (complete_rc == USH_LINENOISE_COMPLETE_EDITED || complete_rc == USH_LINENOISE_COMPLETE_LISTED) {
                ush_history_cancel_nav(sh);
            }
            continue;
        }

        if (ush_is_printable(ch) == 0) {
            continue;
        }

        ush_history_cancel_nav(sh);

        {
            int replaced_selection = ush_input_delete_selection(sh);
            char insert_buf[4];
            u64 insert_len = 1ULL;

            if (sh->line_len + 1ULL >= USH_LINE_MAX) {
                continue;
            }

            insert_buf[0] = ch;
            if ((unsigned char)ch >= 0x80U) {
                insert_len = ush_input_collect_utf8(ch, insert_buf, (u64)sizeof(insert_buf));
            }

            if (sh->line_len + insert_len >= USH_LINE_MAX) {
                continue;
            }

            (void)replaced_selection;

            ush_input_insert_text(sh, insert_buf, insert_len);
            ush_input_render_line(sh);
        }
    }
}
