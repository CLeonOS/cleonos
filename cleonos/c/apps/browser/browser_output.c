#include "browser_internal.h"

#include <stdio.h>

static int ush_browser_ansi_csi_len(const char *text, u64 pos, u64 len, u64 *out_len) {
    u64 i;

    if (text == (const char *)0 || out_len == (u64 *)0 || pos + 1ULL >= len || text[pos] != '\x1B' ||
        text[pos + 1ULL] != '[') {
        return 0;
    }

    i = pos + 2ULL;
    while (i < len) {
        unsigned char ch = (unsigned char)text[i];
        i++;
        if (ch >= 0x40U && ch <= 0x7EU) {
            *out_len = i - pos;
            return 1;
        }
    }

    return 0;
}

static void ush_browser_write_span(const char *text, u64 start, u64 end) {
    u64 i;

    if (text == (const char *)0 || end <= start) {
        return;
    }

    for (i = start; i < end; i++) {
        (void)fputc(text[i], 1);
    }
    (void)fputc('\n', 1);
}

static int ush_browser_print_wrapped_span(const char *text, u64 start, u64 end, u64 *io_line_count) {
    u64 pos = start;

    if (text == (const char *)0 || io_line_count == (u64 *)0 || end <= start) {
        return 1;
    }

    while (pos < end) {
        u64 line_start;
        u64 line_end;
        u64 last_space = (u64)-1;
        u64 col = 0ULL;

        if (pos >= end) {
            break;
        }

        line_start = pos;
        line_end = pos;

        while (pos < end) {
            u64 ansi_len;
            char ch = text[pos];

            if (ush_browser_ansi_csi_len(text, pos, end, &ansi_len) != 0) {
                pos += ansi_len;
                line_end = pos;
                continue;
            }

            if (ch == ' ') {
                last_space = pos;
            }

            if (col >= (u64)USH_BROWSER_OUTPUT_COLS) {
                if (last_space != (u64)-1 && last_space > line_start) {
                    line_end = last_space;
                    pos = last_space + 1ULL;
                }
                break;
            }

            col++;
            pos++;
            line_end = pos;
        }

        while (line_end > line_start && text[line_end - 1ULL] == ' ') {
            line_end--;
        }

        if (line_end > line_start) {
            ush_browser_write_span(text, line_start, line_end);
            (*io_line_count)++;
            if (*io_line_count >= 220ULL) {
                ush_writeln("[browser] output truncated at 220 lines");
                return 0;
            }
        } else if (pos < end) {
            pos++;
        }
    }

    return 1;
}

void ush_browser_print_rendered(const char *source_desc) {
    u64 i = 0ULL;
    u64 line_count = 0ULL;
    u64 k;

    /* 3J clears TTY scrollback, 2J clears screen, H moves cursor home. */
    ush_writeln("\x1B[3J\x1B[2J\x1B[H");
    ush_writeln("[browser] litehtml-gumbo text renderer");
    (void)printf("[browser] source: %s\n", source_desc);
    if (ush_browser_title[0] != '\0') {
        (void)printf("[browser] title: %s\n", ush_browser_title);
    } else {
        ush_writeln("[browser] title: (none)");
    }
    ush_writeln("------------------------------------------------------------");

    while (ush_browser_text_buf[i] != '\0') {
        u64 start = i;

        while (ush_browser_text_buf[i] != '\0' && ush_browser_text_buf[i] != '\n') {
            i++;
        }

        if (i > start) {
            if (ush_browser_print_wrapped_span(ush_browser_text_buf, start, i, &line_count) == 0) {
                break;
            }
        } else if (line_count > 0ULL) {
            ush_writeln("");
        }

        if (ush_browser_text_buf[i] == '\n') {
            i++;
        }
    }

    if (ush_browser_link_count > 0ULL) {
        ush_writeln("");
        ush_writeln("[links]");
        for (k = 0ULL; k < ush_browser_link_count; k++) {
            (void)printf("  [%llu] " USH_BROWSER_ANSI_BLUE_UNDERLINE "%s" USH_BROWSER_ANSI_RESET
                         " -> " USH_BROWSER_ANSI_BLUE_UNDERLINE "%s" USH_BROWSER_ANSI_RESET "\n",
                         (unsigned long long)(k + 1ULL), ush_browser_links[k].text, ush_browser_links[k].href);
        }
    }

    ush_browser_print_forms();
}

static void ush_browser_print_source_line_prefix(u64 line_no) {
    (void)printf("%5llu | ", (unsigned long long)line_no);
}

static void ush_browser_print_source_char(unsigned char ch, u64 *io_col) {
    if (io_col == (u64 *)0) {
        return;
    }

    if (ch == '\t') {
        u64 spaces = 4ULL - (*io_col % 4ULL);
        while (spaces > 0ULL) {
            (void)fputc(' ', 1);
            (*io_col)++;
            spaces--;
        }
        return;
    }

    if (ch >= 0x20U && ch < 0x7FU) {
        (void)fputc((int)ch, 1);
        (*io_col)++;
        return;
    }

    (void)fputc('.', 1);
    (*io_col)++;
}

void ush_browser_print_source(const char *source_desc, const char *html, u64 html_size) {
    u64 i;
    u64 line_no = 1ULL;
    u64 row_count = 0ULL;
    u64 col = 0ULL;
    int line_open = 0;

    /* Keep source view isolated from previous rendered pages and scrollback. */
    ush_writeln("\x1B[3J\x1B[2J\x1B[H");
    ush_writeln("[browser] source view");
    (void)printf("[browser] source: %s\n", (source_desc != (const char *)0) ? source_desc : "(unknown)");
    (void)printf("[browser] bytes: %llu\n", (unsigned long long)html_size);
    ush_writeln("------------------------------------------------------------");

    if (html == (const char *)0 || html_size == 0ULL) {
        ush_writeln("[browser] empty source");
        return;
    }

    for (i = 0ULL; i < html_size; i++) {
        unsigned char ch = (unsigned char)html[i];

        if (line_open == 0) {
            ush_browser_print_source_line_prefix(line_no);
            line_open = 1;
            col = 0ULL;
            row_count++;
            if (row_count > 260ULL) {
                ush_writeln("[browser] source truncated at 260 rows");
                return;
            }
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            (void)fputc('\n', 1);
            line_no++;
            line_open = 0;
            if (line_no > 260ULL) {
                ush_writeln("[browser] source truncated at 260 lines");
                return;
            }
            continue;
        }

        if (col >= (u64)USH_BROWSER_OUTPUT_COLS) {
            (void)fputc('\n', 1);
            ush_browser_print_source_line_prefix(line_no);
            col = 0ULL;
            row_count++;
            if (row_count > 260ULL) {
                ush_writeln("[browser] source truncated at 260 rows");
                return;
            }
        }

        ush_browser_print_source_char(ch, &col);
    }

    if (line_open != 0) {
        (void)fputc('\n', 1);
    }
}
