#include "browser_internal.h"

#include <stdio.h>

static int ush_browser_push_history(char history[][USH_BROWSER_SOURCE_MAX], u64 *io_count, const char *source) {
    u64 i;

    if (history == (char(*)[USH_BROWSER_SOURCE_MAX])0 || io_count == (u64 *)0 || source == (const char *)0 ||
        source[0] == '\0') {
        return 0;
    }

    if (*io_count > 0ULL && ush_streq(history[*io_count - 1ULL], source) != 0) {
        return 1;
    }

    if (*io_count >= (u64)USH_BROWSER_HISTORY_MAX) {
        for (i = 1ULL; i < (u64)USH_BROWSER_HISTORY_MAX; i++) {
            ush_copy(history[i - 1ULL], (u64)USH_BROWSER_SOURCE_MAX, history[i]);
        }
        *io_count = (u64)USH_BROWSER_HISTORY_MAX - 1ULL;
    }

    ush_copy(history[*io_count], (u64)USH_BROWSER_SOURCE_MAX, source);
    (*io_count)++;
    return 1;
}

static int ush_browser_pop_history(char history[][USH_BROWSER_SOURCE_MAX], u64 *io_count, char *out_source,
                                   u64 out_size) {
    if (history == (char(*)[USH_BROWSER_SOURCE_MAX])0 || io_count == (u64 *)0 || out_source == (char *)0 ||
        out_size == 0ULL) {
        return 0;
    }

    if (*io_count == 0ULL) {
        return 0;
    }

    (*io_count)--;
    ush_copy(out_source, out_size, history[*io_count]);
    history[*io_count][0] = '\0';
    return 1;
}

static int ush_browser_parse_link_index(const char *text, u64 *out_index) {
    u64 i = 0ULL;
    u64 value = 0ULL;

    if (text == (const char *)0 || out_index == (u64 *)0 || text[0] == '\0') {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10ULL + (u64)(text[i] - '0');
        i++;
    }

    if (value == 0ULL) {
        return 0;
    }

    *out_index = value - 1ULL;
    return 1;
}

static int ush_browser_parse_u64_arg(const char *text, u64 *out_value) {
    char first[USH_BROWSER_INPUT_MAX];
    const char *rest = "";

    if (text == (const char *)0 || out_value == (u64 *)0) {
        return 0;
    }

    if (ush_split_first_and_rest(text, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    return ush_parse_u64_dec(first, out_value);
}

static int ush_browser_has_prefix(const char *text, const char *prefix) {
    u64 i = 0ULL;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static void ush_browser_strip_view_source(char *io_source, int *out_source_mode) {
    static const char prefix[] = "view-source:";
    u64 prefix_len = (u64)(sizeof(prefix) - 1U);
    u64 i = 0ULL;

    if (io_source == (char *)0 || out_source_mode == (int *)0) {
        return;
    }

    if (ush_browser_has_prefix(io_source, prefix) == 0) {
        return;
    }

    while (io_source[prefix_len + i] != '\0') {
        io_source[i] = io_source[prefix_len + i];
        i++;
    }
    io_source[i] = '\0';
    *out_source_mode = 1;
}

static int ush_browser_parse_args(const char *arg, char *out_source, u64 out_source_size, int *out_source_mode) {
    char first[USH_PATH_MAX];
    const char *rest = "";

    if (out_source == (char *)0 || out_source_size == 0ULL || out_source_mode == (int *)0) {
        return 0;
    }

    out_source[0] = '\0';
    *out_source_mode = 0;

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    if (ush_streq(first, "--source") != 0 || ush_streq(first, "-s") != 0) {
        if (rest == (const char *)0 || rest[0] == '\0') {
            return 0;
        }
        if (ush_split_first_and_rest(rest, first, (u64)sizeof(first), &rest) == 0) {
            return 0;
        }
        *out_source_mode = 1;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        return 0;
    }

    ush_copy(out_source, out_source_size, first);
    ush_browser_strip_view_source(out_source, out_source_mode);
    return (out_source[0] != '\0') ? 1 : 0;
}

static void ush_browser_usage(void) {
    ush_writeln("usage: browser [--source|-s] <file.html|http://...|https://...>");
    ush_writeln("       browser view-source:<file.html|http://...|https://...>");
    ush_writeln("note: parser is gumbo from litehtml (no handwritten parser)");
}

int ush_browser_run_session(const ush_state *sh, const char *arg) {
    char source[USH_BROWSER_SOURCE_MAX];
    char current_source[USH_BROWSER_SOURCE_MAX];
    char next_source[USH_BROWSER_SOURCE_MAX];
    char input_line[USH_BROWSER_INPUT_MAX];
    char post_body[USH_BROWSER_FORM_BODY_MAX];
    char pending_body[USH_BROWSER_FORM_BODY_MAX];
    char history[USH_BROWSER_HISTORY_MAX][USH_BROWSER_SOURCE_MAX];
    u64 history_count = 0ULL;
    int parse_ret;
    u64 html_size = 0ULL;
    u64 pending_body_len = 0ULL;
    int loaded_once = 0;
    int source_mode = 0;
    int pending_post = 0;
    u64 i;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    if (ush_browser_ensure_buffers() == 0) {
        ush_writeln("browser: memory allocation failed");
        return 0;
    }

    for (i = 0ULL; i < (u64)USH_BROWSER_HISTORY_MAX; i++) {
        history[i][0] = '\0';
    }
    post_body[0] = '\0';
    pending_body[0] = '\0';

    parse_ret = ush_browser_parse_args(arg, source, (u64)sizeof(source), &source_mode);
    if (parse_ret == 2) {
        ush_browser_usage();
        return 1;
    }

    if (parse_ret == 0) {
        ush_browser_usage();
        return 0;
    }

    if (ush_browser_is_http_url(source) != 0 || ush_browser_is_https_url(source) != 0) {
        ush_copy(current_source, (u64)sizeof(current_source), source);
    } else {
        if (ush_resolve_path(sh, source, current_source, (u64)sizeof(current_source)) == 0) {
            ush_copy(current_source, (u64)sizeof(current_source), source);
        }
    }

    for (;;) {
        if (ush_browser_load_request(sh, current_source, pending_post != 0 ? "POST" : "GET", pending_body,
                                     pending_body_len, ush_browser_html_buf, (u64)USH_BROWSER_HTML_BUF_CAP,
                                     &html_size) == 0) {
            if (ush_browser_is_http_url(current_source) != 0 || ush_browser_is_https_url(current_source) != 0) {
                (void)printf("browser: URL fetch failed: %s\n", ush_browser_fetch_last_error());
            } else {
                ush_writeln("browser: file read failed");
            }

            if (loaded_once == 0 ||
                ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                return 0;
            }
            ush_writeln("browser: returned to previous page");
            pending_post = 0;
            pending_body_len = 0ULL;
            pending_body[0] = '\0';
            continue;
        }
        pending_post = 0;
        pending_body_len = 0ULL;
        pending_body[0] = '\0';

        if (source_mode == 0 && ush_browser_render_html(ush_browser_html_buf, html_size) == 0) {
            ush_writeln("browser: parse/render failed");
            if (loaded_once == 0 ||
                ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) == 0) {
                return 0;
            }
            ush_writeln("browser: returned to previous page");
            continue;
        }

        loaded_once = 1;
        if (source_mode != 0) {
            ush_browser_print_source(current_source, ush_browser_html_buf, html_size);
        } else {
            ush_browser_print_rendered(current_source);
        }

        for (;;) {
            ush_writeln("");
            ush_writeln("[browser] interactive mode");
            ush_writeln("[browser] <number>: open link   o <src>: open URL/path   set <field> <value>   submit <form>");
            ush_writeln("[browser] forms: list forms      v: source/render        b: back   r: reload   q: quit");
            (void)fputs("browser> ", 1);

            if (ush_browser_read_line(input_line, (u64)sizeof(input_line)) == 0) {
                return 1;
            }
            ush_trim_line(input_line);

            if (input_line[0] == '\0') {
                continue;
            }

            if (ush_streq(input_line, "q") != 0 || ush_streq(input_line, "quit") != 0 ||
                ush_streq(input_line, "exit") != 0) {
                return 1;
            }

            if (ush_streq(input_line, "r") != 0 || ush_streq(input_line, "reload") != 0) {
                pending_post = 0;
                pending_body_len = 0ULL;
                pending_body[0] = '\0';
                break;
            }

            if (ush_streq(input_line, "v") != 0 || ush_streq(input_line, "view") != 0 ||
                ush_streq(input_line, "source") != 0 || ush_streq(input_line, "html") != 0) {
                source_mode = (source_mode == 0) ? 1 : 0;
                break;
            }

            if (ush_streq(input_line, "render") != 0 || ush_streq(input_line, "page") != 0) {
                source_mode = 0;
                break;
            }

            if (ush_streq(input_line, "b") != 0 || ush_streq(input_line, "back") != 0) {
                if (ush_browser_pop_history(history, &history_count, current_source, (u64)sizeof(current_source)) ==
                    0) {
                    ush_writeln("browser: no history");
                    continue;
                }
                pending_post = 0;
                pending_body_len = 0ULL;
                pending_body[0] = '\0';
                break;
            }

            if (ush_streq(input_line, "h") != 0 || ush_streq(input_line, "help") != 0) {
            ush_writeln("[browser] commands:");
            ush_writeln("  <number>      open link by index");
            ush_writeln("  o <src>       open new URL/path");
            ush_writeln("  forms         list forms and fields");
            ush_writeln("  set <n> <v>   set form field value");
            ush_writeln("  submit <n>    submit form by index");
            ush_writeln("  cookies       list cookie/token state");
            ush_writeln("  cookie clear  clear cookies");
            ush_writeln("  token set <v> set Authorization Bearer token");
            ush_writeln("  token clear   clear Bearer token");
            ush_writeln("  v             toggle source/render view");
                ush_writeln("  source        toggle source/render view");
                ush_writeln("  render        switch back to rendered page");
                ush_writeln("  b             back");
                ush_writeln("  r             reload");
                ush_writeln("  q             quit");
                continue;
            }

            if (ush_streq(input_line, "forms") != 0 || ush_streq(input_line, "fields") != 0) {
                if (source_mode != 0) {
                    ush_writeln("browser: forms are available in rendered mode");
                } else {
                    ush_browser_print_forms();
                }
                continue;
            }

            if (ush_streq(input_line, "cookies") != 0 || ush_streq(input_line, "cookie") != 0 ||
                ush_streq(input_line, "auth") != 0) {
                ush_browser_cookie_print_state();
                continue;
            }

            if (ush_streq(input_line, "cookie clear") != 0 || ush_streq(input_line, "cookies clear") != 0) {
                ush_browser_cookie_clear_all();
                ush_writeln("browser: cookies cleared");
                continue;
            }

            if ((input_line[0] == 't' && input_line[1] == 'o' && input_line[2] == 'k' && input_line[3] == 'e' &&
                 input_line[4] == 'n' && input_line[5] == ' ') ||
                (input_line[0] == 'a' && input_line[1] == 'u' && input_line[2] == 't' && input_line[3] == 'h' &&
                 input_line[4] == ' ')) {
                const char *payload = (input_line[0] == 't') ? (input_line + 6) : (input_line + 5);
                char subcmd[32];
                const char *value = "";

                if (ush_split_first_and_rest(payload, subcmd, (u64)sizeof(subcmd), &value) == 0) {
                    ush_writeln("browser: usage token set <value> | token clear");
                    continue;
                }
                if (ush_streq(subcmd, "clear") != 0) {
                    ush_browser_cookie_clear_token();
                    ush_writeln("browser: bearer token cleared");
                    continue;
                }
                if (ush_streq(subcmd, "set") != 0) {
                    while (*value == ' ') {
                        value++;
                    }
                    if (value[0] == '\0') {
                        ush_writeln("browser: token value is empty");
                        continue;
                    }
                    ush_browser_cookie_set_token(value);
                    ush_writeln("browser: bearer token set");
                    continue;
                }
                ush_writeln("browser: usage token set <value> | token clear");
                continue;
            }

            if ((input_line[0] == 's' && input_line[1] == 'e' && input_line[2] == 't' && input_line[3] == ' ') ||
                (input_line[0] == 'e' && input_line[1] == 'd' && input_line[2] == 'i' && input_line[3] == 't' &&
                 input_line[4] == ' ')) {
                const char *payload = (input_line[0] == 's') ? (input_line + 4) : (input_line + 5);
                char field_arg[32];
                const char *value = "";
                u64 field_number = 0ULL;

                if (source_mode != 0) {
                    ush_writeln("browser: form editing is available in rendered mode");
                    continue;
                }
                if (ush_split_first_and_rest(payload, field_arg, (u64)sizeof(field_arg), &value) == 0 ||
                    ush_parse_u64_dec(field_arg, &field_number) == 0 || value == (const char *)0) {
                    ush_writeln("browser: usage set <field> <value>");
                    continue;
                }
                while (*value == ' ') {
                    value++;
                }
                if (ush_browser_form_set_value(field_number, value) == 0) {
                    ush_writeln("browser: field not editable");
                    continue;
                }
                ush_writeln("browser: field updated");
                ush_browser_print_forms();
                continue;
            }

            if ((input_line[0] == 's' && input_line[1] == 'u' && input_line[2] == 'b' && input_line[3] == 'm' &&
                 input_line[4] == 'i' && input_line[5] == 't' &&
                 (input_line[6] == '\0' || input_line[6] == ' ')) ||
                (input_line[0] == 'p' && input_line[1] == 'o' && input_line[2] == 's' && input_line[3] == 't' &&
                 (input_line[4] == '\0' || input_line[4] == ' '))) {
                const char *payload = (input_line[0] == 'p') ? (input_line + 4) : (input_line + 6);
                u64 form_number = 1ULL;
                int is_post = 0;

                while (*payload == ' ') {
                    payload++;
                }
                if (payload[0] != '\0' && ush_browser_parse_u64_arg(payload, &form_number) == 0) {
                    ush_writeln("browser: usage submit <form>");
                    continue;
                }
                if (source_mode != 0) {
                    ush_writeln("browser: forms are available in rendered mode");
                    continue;
                }
                if (ush_browser_form_submit(current_source, form_number, next_source, (u64)sizeof(next_source),
                                            post_body, (u64)sizeof(post_body), &pending_body_len, &is_post) == 0) {
                    ush_writeln("browser: form submit failed");
                    continue;
                }
                if (is_post != 0) {
                    ush_copy(pending_body, (u64)sizeof(pending_body), post_body);
                    pending_post = 1;
                } else {
                    pending_body[0] = '\0';
                    pending_body_len = 0ULL;
                    pending_post = 0;
                }
                (void)ush_browser_push_history(history, &history_count, current_source);
                ush_copy(current_source, (u64)sizeof(current_source), next_source);
                source_mode = 0;
                break;
            }

            ush_zero(next_source, (u64)sizeof(next_source));
            if ((input_line[0] == 'o' && input_line[1] == ' ') ||
                (input_line[0] == 'o' && input_line[1] == 'p' && input_line[2] == 'e' && input_line[3] == 'n' &&
                 input_line[4] == ' ')) {
                const char *payload = (input_line[1] == ' ') ? (input_line + 2) : (input_line + 5);
                while (*payload == ' ') {
                    payload++;
                }
                ush_copy(next_source, (u64)sizeof(next_source), payload);
            } else {
                u64 link_index = 0ULL;
                if (ush_browser_parse_link_index(input_line, &link_index) != 0) {
                    if (source_mode != 0) {
                        ush_writeln("browser: link indexes are available in rendered mode");
                        continue;
                    }

                    if (link_index >= ush_browser_link_count) {
                        ush_writeln("browser: link index out of range");
                        continue;
                    }

                    if (ush_browser_resolve_href(current_source, ush_browser_links[link_index].href, next_source,
                                                 (u64)sizeof(next_source)) == 0) {
                        ush_writeln("browser: cannot resolve link target");
                        continue;
                    }
                } else {
                    ush_copy(next_source, (u64)sizeof(next_source), input_line);
                }
            }

            if (next_source[0] == '\0') {
                ush_writeln("browser: empty target");
                continue;
            }

            if (ush_browser_is_http_url(next_source) == 0 && ush_browser_is_https_url(next_source) == 0 &&
                next_source[0] != '/') {
                char resolved_target[USH_BROWSER_SOURCE_MAX];

                if (ush_browser_resolve_href(current_source, next_source, resolved_target,
                                             (u64)sizeof(resolved_target)) != 0) {
                    ush_copy(next_source, (u64)sizeof(next_source), resolved_target);
                } else if (ush_resolve_path(sh, next_source, resolved_target, (u64)sizeof(resolved_target)) != 0) {
                    ush_copy(next_source, (u64)sizeof(next_source), resolved_target);
                }
            }

            if (ush_streq(next_source, current_source) != 0) {
                continue;
            }

            (void)ush_browser_push_history(history, &history_count, current_source);
            ush_copy(current_source, (u64)sizeof(current_source), next_source);
            source_mode = 0;
            pending_post = 0;
            pending_body_len = 0ULL;
            pending_body[0] = '\0';
            break;
        }
    }
}
